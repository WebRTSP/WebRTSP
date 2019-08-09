#include "Client.h"

#include <deque>

#include "CxxPtr/libwebsocketsPtr.h"

#include "Common/MessageBuffer.h"


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

    lws* connection;
    bool connected;
};

struct SessionData
{
    MessageBuffer incomingMessage;
    std::deque<MessageBuffer> sendMessages;
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

static bool OnConnected(ContextData* cd, SessionContextData* scd)
{
    MessageBuffer message;
    message.assign(
        "OPTIONS * RTSP/1.0\r\n"
        "CSeq: 1\r\n");

    Send(scd, &message);

    return true;
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
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lwsl_notice("Connection to server established\n");

            scd->data = new SessionData;
            scd->wsi = wsi;

            cd->connected = true;

            if(!OnConnected(cd, scd))
                return -1;

            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if(scd->data->incomingMessage.onReceive(wsi, in, len)) {
                lwsl_notice("%.*s\n", static_cast<int>(scd->data->incomingMessage.size()), scd->data->incomingMessage.data());

                scd->data->incomingMessage.clear();
            }

            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
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

            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("Can not connect to server\n");

            delete scd->data;
            scd = nullptr;

            cd->connection = nullptr;
            cd->connected = false;

            break;
        default:
            break;
    }

    return 0;
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

    time_t disconnectedTime = 1; // =1 to emulate timeout on startup

    while(lws_service(context, 50) >= 0) {
        if(!contextData.connection) {
            struct timespec now;
            if(0 == clock_gettime(CLOCK_MONOTONIC, &now)) {
                if(!disconnectedTime) {
                    disconnectedTime = now.tv_sec;
                } else if(now.tv_sec - disconnectedTime > RECONNECT_TIMEOUT) {
                    Connect(context);
                    disconnectedTime = 0;
                }
            }
        }
    }

    return true;
}

}
