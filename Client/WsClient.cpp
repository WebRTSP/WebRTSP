#include "WsClient.h"

#include <deque>

#include "CxxPtr/libwebsocketsPtr.h"

#include "Common/MessageBuffer.h"
#include "Common/LwsSource.h"
#include "RtspParser/RtspSerialize.h"
#include "RtspParser/RtspParser.h"


namespace client {

namespace {

enum {
    RX_BUFFER_SIZE = 512,
};

enum {
    PROTOCOL_ID,
};

#if LWS_LIBRARY_VERSION_MAJOR < 3
enum {
    LWS_CALLBACK_CLIENT_CLOSED = LWS_CALLBACK_CLOSED
};
#endif

struct SessionData
{
    bool terminateSession = false;
    MessageBuffer incomingMessage;
    std::deque<MessageBuffer> sendMessages;
    std::unique_ptr<rtsp::ClientSession > rtspSession;
};

// Should contain only POD types,
// since created inside libwebsockets on session create.
struct SessionContextData
{
    lws* wsi;
    SessionData* data;
};

}

struct WsClient::Private
{
    Private(
        WsClient*,
        const Config&,
        GMainLoop*,
        const CreateSession&,
        const Disconnected&);

    bool init();
    int httpCallback(lws*, lws_callback_reasons, void* user, void* in, size_t len);
    int wsCallback(lws*, lws_callback_reasons, void* user, void* in, size_t len);
    bool onMessage(SessionContextData*, const MessageBuffer&);

    void send(SessionContextData*, MessageBuffer*);
    void sendRequest(SessionContextData*, const rtsp::Request*);
    void sendResponse(SessionContextData*, const rtsp::Response*);

    void connect();
    bool onConnected(SessionContextData* scd);


    WsClient *const owner;
    Config config;
    GMainLoop* loop = nullptr;
    CreateSession createSession;
    Disconnected disconnected;

    LwsSourcePtr lwsSourcePtr;
    LwsContextPtr contextPtr;

    lws* connection = nullptr;
    bool connected = false;
};

WsClient::Private::Private(
    WsClient* owner,
    const Config& config,
    GMainLoop* loop,
    const WsClient::CreateSession& createSession,
    const Disconnected& disconnected) :
    owner(owner), config(config), loop(loop),
    createSession(createSession), disconnected(disconnected)
{
}

int WsClient::Private::wsCallback(
    lws* wsi,
    lws_callback_reasons reason,
    void* user,
    void* in, size_t len)
{
    SessionContextData* scd = static_cast<SessionContextData*>(user);
    switch(reason) {
        case LWS_CALLBACK_ADD_POLL_FD:
        case LWS_CALLBACK_DEL_POLL_FD:
        case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
            return LwsSourceCallback(lwsSourcePtr, wsi, reason, in, len);
        case LWS_CALLBACK_CLIENT_ESTABLISHED: {
            lwsl_notice("Connection to server established\n");

            std::unique_ptr<rtsp::ClientSession> session =
                createSession(
                    std::bind(&Private::sendRequest, this, scd, std::placeholders::_1),
                    std::bind(&Private::sendResponse, this, scd, std::placeholders::_1));
            if(!session)
                return -1;

            scd->data =
                new SessionData {
                    .incomingMessage ={},
                    .sendMessages = {},
                    .rtspSession = std::move(session)};
            scd->wsi = wsi;

            connected = true;

            if(!onConnected(scd))
                return -1;

            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if(scd->data->incomingMessage.onReceive(wsi, in, len)) {
                lwsl_notice("-> Client: %.*s\n", static_cast<int>(scd->data->incomingMessage.size()), scd->data->incomingMessage.data());

                if(!onMessage(scd, scd->data->incomingMessage))
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

            connection = nullptr;
            connected = false;

            if(disconnected)
                disconnected();

            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("Can not connect to server\n");

            delete scd->data;
            scd = nullptr;

            connection = nullptr;
            connected = false;

            if(disconnected)
                disconnected();

            break;
        default:
            break;
    }

    return 0;
}

bool WsClient::Private::init()
{
    auto WsCallback =
        [] (lws* wsi, lws_callback_reasons reason, void* user, void* in, size_t len) -> int {
            lws_context* context = lws_get_context(wsi);
            Private* p = static_cast<Private*>(lws_context_user(context));

            return p->wsCallback(wsi, reason, user, in, len);
        };

    static const lws_protocols protocols[] = {
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

    lws_context_creation_info wsInfo {};
    wsInfo.gid = -1;
    wsInfo.uid = -1;
    wsInfo.port = CONTEXT_PORT_NO_LISTEN;
    wsInfo.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    wsInfo.protocols = protocols;
    wsInfo.user = this;

    contextPtr.reset(lws_create_context(&wsInfo));
    lws_context* context = contextPtr.get();
    if(!context)
        return false;

    lwsSourcePtr = LwsSourceNew(context, g_main_context_get_thread_default());
    if(!lwsSourcePtr)
        return false;

    return true;
}

void WsClient::Private::connect()
{
    if(connection)
        return;

    if(config.server.empty() || !config.serverPort) {
        lwsl_err("Missing required connect parameter.\n");
        return;
    }

    char hostAndPort[config.server.size() + 1 + 5 + 1];
    snprintf(hostAndPort, sizeof(hostAndPort), "%s:%u",
        config.server.c_str(), config.serverPort);

    lwsl_notice("Connecting to %s... \n", hostAndPort);

    struct lws_client_connect_info connectInfo = {};
    connectInfo.context = contextPtr.get();
    connectInfo.address = config.server.c_str();
    connectInfo.port = config.serverPort;
    connectInfo.path = "/";
    connectInfo.protocol = "webrtsp";
    connectInfo.host = hostAndPort;

    connection = lws_client_connect_via_info(&connectInfo);
    connected = false;
}

bool WsClient::Private::onConnected(SessionContextData* scd)
{
    scd->data->rtspSession->onConnected();

    return true;
}

bool WsClient::Private::onMessage(
    SessionContextData* scd,
    const MessageBuffer& message)
{
    if(rtsp::IsRequest(message.data(), message.size())) {
        std::unique_ptr<rtsp::Request> requestPtr =
            std::make_unique<rtsp::Request>();
        if(!rtsp::ParseRequest(message.data(), message.size(), requestPtr.get()))
            return false;

        if(!scd->data->rtspSession->handleRequest(requestPtr))
            return false;
    } else {
        rtsp::Response response;
        if(!rtsp::ParseResponse(message.data(), message.size(), &response))
            return false;

        if(!scd->data->rtspSession->handleResponse(response))
            return false;
    }

    return true;
}

void WsClient::Private::send(SessionContextData* scd, MessageBuffer* message)
{
    scd->data->sendMessages.emplace_back(std::move(*message));

    lws_callback_on_writable(scd->wsi);
}

void WsClient::Private::sendRequest(
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

    send(scd, &requestMessage);
}

void WsClient::Private::sendResponse(
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

    send(scd, &responseMessage);
}

WsClient::WsClient(
    const Config& config,
    GMainLoop* loop,
    const CreateSession& createSession,
    const Disconnected& disconnected) noexcept:
    _p(std::make_unique<Private>(this, config, loop, createSession, disconnected))
{
}

WsClient::~WsClient()
{
}

bool WsClient::init() noexcept
{
    return _p->init();
}

void WsClient::connect() noexcept
{
    _p->connect();
}

}
