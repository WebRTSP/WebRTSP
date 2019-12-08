#include "Client.h"

#include <deque>

#include "CxxPtr/libwebsocketsPtr.h"
#include <CxxPtr/GlibPtr.h>

#include "Common/MessageBuffer.h"
#include "Common/LwsSource.h"
#include "RtspParser/RtspSerialize.h"
#include "RtspParser/RtspParser.h"

#include "ClientSession.h"


namespace client {

enum {
    RX_BUFFER_SIZE = 512,
    RECONNECT_TIMEOUT = 5,
};

enum {
    PROTOCOL_ID,
};

#if LWS_LIBRARY_VERSION_MAJOR < 3
enum {
    LWS_CALLBACK_CLIENT_CLOSED = LWS_CALLBACK_CLOSED
};
#endif

namespace {

struct ContextData
{
    const Config *const config;
    LwsSourcePtr lwsSource;

    lws* connection;
    bool connected;
};

struct SessionData
{
    bool terminateSession = false;
    MessageBuffer incomingMessage;
    std::deque<MessageBuffer> sendMessages;
    ClientSession rtspSession;
};

// Should contain only POD types,
// since created inside libwebsockets on session create.
struct SessionContextData
{
    lws* wsi;
    SessionData* data;
};

}

static void Send(SessionContextData* scd, MessageBuffer* message)
{
    scd->data->sendMessages.emplace_back(std::move(*message));

    lws_callback_on_writable(scd->wsi);
}

static void SendRequest(
    ContextData* cd,
    SessionContextData* scd,
    const rtsp::Request* request)
{
    if(!request) {
        scd->data->terminateSession = true;
        lws_callback_on_writable(scd->wsi);
        return;
    }

    MessageBuffer requestMessage;
    requestMessage.assign(rtsp::Serialize(*request));

    if(requestMessage.empty()) {
        scd->data->terminateSession = true;
        lws_callback_on_writable(scd->wsi);
        return;
    }

    Send(scd, &requestMessage);
}

static void SendResponse(
    ContextData* cd,
    SessionContextData* scd,
    const rtsp::Response* response)
{
    if(!response) {
        scd->data->terminateSession = true;
        lws_callback_on_writable(scd->wsi);
        return;
    }

    MessageBuffer responseMessage;
    responseMessage.assign(rtsp::Serialize(*response));
    if(responseMessage.empty()) {
        scd->data->terminateSession = true;
        lws_callback_on_writable(scd->wsi);
        return;
    }

    Send(scd, &responseMessage);
}

static bool OnConnected(ContextData* cd, SessionContextData* scd)
{
    scd->data->rtspSession.onConnected();

    return true;
}

static bool OnMessage(
    ContextData* cd,
    SessionContextData* scd,
    const MessageBuffer& message)
{
    if(rtsp::IsRequest(message.data(), message.size())) {
        std::unique_ptr<rtsp::Request> requestPtr =
            std::make_unique<rtsp::Request>();
        if(!rtsp::ParseRequest(message.data(), message.size(), requestPtr.get()))
            return false;

        if(!scd->data->rtspSession.handleRequest(requestPtr))
            return false;
    } else {
        rtsp::Response response;
        if(!rtsp::ParseResponse(message.data(), message.size(), &response))
            return false;

        if(!scd->data->rtspSession.handleResponse(response))
            return false;
    }

    return true;
}

static void Connect(struct lws_context* context)
{
    ContextData* cd = static_cast<ContextData*>(lws_context_user(context));

    if(cd->connection)
        return;

    if(cd->config->server.empty() || !cd->config->serverPort) {
        lwsl_err("Missing required connect parameter.\n");
        return;
    }

    char hostAndPort[cd->config->server.size() + 1 + 5 + 1];
    snprintf(hostAndPort, sizeof(hostAndPort), "%s:%u",
        cd->config->server.c_str(), cd->config->serverPort);

    lwsl_notice("Connecting to %s... \n", hostAndPort);

    struct lws_client_connect_info connectInfo = {};
    connectInfo.context = context;
    connectInfo.address = cd->config->server.c_str();
    connectInfo.port = cd->config->serverPort;
    connectInfo.path = "/";
    connectInfo.protocol = "webrtsp";
    connectInfo.host = hostAndPort;

    cd->connection = lws_client_connect_via_info(&connectInfo);
    cd->connected = false;
}

static void ScheduleReconnect(lws_context* context)
{
    g_timeout_add_seconds(
        RECONNECT_TIMEOUT,
        [] (gpointer userData) -> gboolean {
            Connect(static_cast<lws_context*>(userData));
            return false;
        }, context);
}

static int WsCallback(
    lws* wsi,
    lws_callback_reasons reason,
    void* user,
    void* in, size_t len)
{
    lws_context* context = lws_get_context(wsi);
    ContextData* cd = static_cast<ContextData*>(lws_context_user(context));
    SessionContextData* scd = static_cast<SessionContextData*>(user);

    switch (reason) {
        case LWS_CALLBACK_ADD_POLL_FD:
        case LWS_CALLBACK_DEL_POLL_FD:
        case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
            return LwsSourceCallback(cd->lwsSource, wsi, reason, in, len);
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lwsl_notice("Connection to server established\n");

            scd->data =
                new SessionData {
                    .incomingMessage ={},
                    .sendMessages = {},
                    .rtspSession = {
                        std::bind(SendRequest, cd, scd, std::placeholders::_1),
                        std::bind(SendResponse, cd, scd, std::placeholders::_1) }
                };
            scd->wsi = wsi;

            cd->connected = true;

            if(!OnConnected(cd, scd))
                return -1;

            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if(scd->data->incomingMessage.onReceive(wsi, in, len)) {
                lwsl_notice("-> Client: %.*s\n", static_cast<int>(scd->data->incomingMessage.size()), scd->data->incomingMessage.data());

                if(!OnMessage(cd, scd, scd->data->incomingMessage))
                    return -1;

                scd->data->incomingMessage.clear();
            }

            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if(scd->data->terminateSession)
                return -1;

            if(!scd->data->sendMessages.empty()) {
                MessageBuffer& buffer = scd->data->sendMessages.front();
                if(!buffer.writeAsText(wsi)) {
                    lwsl_err("Write failed\n");
                    return -1;
                }

                scd->data->sendMessages.pop_front();

                if(!scd->data->sendMessages.empty())
                    lws_callback_on_writable(wsi);
            }

            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            lwsl_notice("Connection to server is closed\n");

            delete scd->data;
            scd = nullptr;

            cd->connection = nullptr;
            cd->connected = false;

            ScheduleReconnect(context);

            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("Can not connect to server\n");

            delete scd->data;
            scd = nullptr;

            cd->connection = nullptr;
            cd->connected = false;

            ScheduleReconnect(context);

            break;
        default:
            break;
    }

    return 0;
}

bool Client(const Config* config) noexcept
{
    const lws_protocols protocols[] = {
        {
            "webrtsp",
            WsCallback,
            sizeof(SessionContextData),
            RX_BUFFER_SIZE,
            PROTOCOL_ID,
            nullptr
        },
        { nullptr, nullptr, 0, 0, 0, nullptr } /* terminator */
    };

    GMainLoopPtr loopPtr(g_main_loop_new(nullptr, FALSE));
    GMainLoop* loop = loopPtr.get();

    ContextData contextData {
        .config = config
    };

    lws_context_creation_info wsInfo {};
    wsInfo.gid = -1;
    wsInfo.uid = -1;
    wsInfo.port = CONTEXT_PORT_NO_LISTEN;
    wsInfo.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    wsInfo.protocols = protocols;
    wsInfo.user = &contextData;

    LwsContextPtr contextPtr(lws_create_context(&wsInfo));
    lws_context* context = contextPtr.get();
    if(!context)
        return false;

    contextData.lwsSource = LwsSourceNew(context);
    if(!contextData.lwsSource)
        return false;

    Connect(context);

    g_main_loop_run(loop);

    return true;
}

}
