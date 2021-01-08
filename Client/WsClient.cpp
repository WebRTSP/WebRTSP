#include "WsClient.h"

#include <deque>
#include <algorithm>

#include "CxxPtr/libwebsocketsPtr.h"

#include "Common/MessageBuffer.h"
#include "Common/LwsSource.h"
#include "RtspParser/RtspSerialize.h"
#include "RtspParser/RtspParser.h"

#include "Log.h"


namespace client {

namespace {

enum {
    RX_BUFFER_SIZE = 512,
    PING_INTERVAL = 20,
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
    std::unique_ptr<rtsp::Session > rtspSession;
};

// Should contain only POD types,
// since created inside libwebsockets on session create.
struct SessionContextData
{
    lws* wsi;
    SessionData* data;
};

const auto Log = WsClientLog;

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
    bool onConnected(SessionContextData*);


    WsClient *const owner;
    Config config;
    GMainLoop* loop = nullptr;
    CreateSession createSession;
    Disconnected disconnected;

#if !defined(LWS_WITH_GLIB)
    LwsSourcePtr lwsSourcePtr;
#endif
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
#if !defined(LWS_WITH_GLIB)
        case LWS_CALLBACK_ADD_POLL_FD:
        case LWS_CALLBACK_DEL_POLL_FD:
        case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
            return LwsSourceCallback(lwsSourcePtr, wsi, reason, in, len);
#endif
        case LWS_CALLBACK_CLIENT_ESTABLISHED: {
            Log()->info("Connection to server established.");

            std::unique_ptr<rtsp::Session> session =
                createSession(
                    std::bind(&Private::sendRequest, this, scd, std::placeholders::_1),
                    std::bind(&Private::sendResponse, this, scd, std::placeholders::_1));
            if(!session)
                return -1;

            scd->data =
                new SessionData {
                    .terminateSession = false,
                    .incomingMessage ={},
                    .sendMessages = {},
                    .rtspSession = std::move(session)};
            scd->wsi = wsi;

            connected = true;

            if(!onConnected(scd))
                return -1;

            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            Log()->trace("PONG");
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if(scd->data->incomingMessage.onReceive(wsi, in, len)) {
                if(Log()->level() <= spdlog::level::trace) {
                    std::string logMessage;
                    logMessage.reserve(scd->data->incomingMessage.size());
                    std::remove_copy(
                        scd->data->incomingMessage.data(),
                        scd->data->incomingMessage.data() + scd->data->incomingMessage.size(),
                        std::back_inserter(logMessage), '\r');

                    Log()->trace("-> WsClient: {}", logMessage);
                }

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
                    Log()->error("Write failed.");
                    return -1;
                }

                scd->data->sendMessages.pop_front();

                if(!scd->data->sendMessages.empty())
                    lws_callback_on_writable(wsi);
            }

            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            Log()->info("Connection to server is closed.");

            delete scd->data;
            scd = nullptr;

            connection = nullptr;
            connected = false;

            if(disconnected)
                disconnected();

            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            Log()->error("Can not connect to server.");

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
#if defined(LWS_WITH_GLIB)
    wsInfo.options |= LWS_SERVER_OPTION_GLIB;
    wsInfo.foreign_loops = reinterpret_cast<void**>(&loop);
#endif
    wsInfo.protocols = protocols;
#if LWS_LIBRARY_VERSION_NUMBER < 4000000
    wsInfo.ws_ping_pong_interval = PING_INTERVAL;
#else
    lws_retry_bo_t retryPolicy {};
    retryPolicy.secs_since_valid_ping = PING_INTERVAL;
    wsInfo.retry_and_idle_policy = &retryPolicy;
#endif
    wsInfo.user = this;

    contextPtr.reset(lws_create_context(&wsInfo));
    lws_context* context = contextPtr.get();
    if(!context)
        return false;

#if !defined(LWS_WITH_GLIB)
    lwsSourcePtr = LwsSourceNew(context, g_main_context_get_thread_default());
    if(!lwsSourcePtr)
        return false;
#endif

    return true;
}

void WsClient::Private::connect()
{
    if(connection)
        return;

    if(config.server.empty() || !config.serverPort) {
        Log()->error("Missing required connect parameter.");
        return;
    }

    char hostAndPort[config.server.size() + 1 + 5 + 1];
    snprintf(hostAndPort, sizeof(hostAndPort), "%s:%u",
        config.server.c_str(), config.serverPort);

    Log()->info("Connecting to {}...", &hostAndPort[0]);

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
    return scd->data->rtspSession->onConnected();
}

bool WsClient::Private::onMessage(
    SessionContextData* scd,
    const MessageBuffer& message)
{
    if(rtsp::IsRequest(message.data(), message.size())) {
        std::unique_ptr<rtsp::Request> requestPtr =
            std::make_unique<rtsp::Request>();
        if(!rtsp::ParseRequest(message.data(), message.size(), requestPtr.get())) {
            Log()->error("Fail parse request. Forcing session disconnect...");
            return false;
        }

        if(!scd->data->rtspSession->handleRequest(requestPtr)) {
            Log()->debug("Fail handle request. Forcing session disconnect...");
            return false;
        }
    } else {
        std::unique_ptr<rtsp::Response > responsePtr =
            std::make_unique<rtsp::Response>();
        if(!rtsp::ParseResponse(message.data(), message.size(), responsePtr.get())) {
            Log()->error("Fail parse response. Forcing session disconnect...");
            return false;
        }

        if(!scd->data->rtspSession->handleResponse(responsePtr)) {
            Log()->error("Fail handle response. Forcing session disconnect...");
            return false;
        }
    }

    return true;
}

void WsClient::Private::send(SessionContextData* scd, MessageBuffer* message)
{
    assert(!message->empty());

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

    const std::string serializedRequest = rtsp::Serialize(*request);
    if(serializedRequest.empty()) {
        scd->data->terminateSession = true;
        lws_callback_on_writable(scd->wsi);
    } else {
        if(Log()->level() <= spdlog::level::trace) {
            std::string logMessage;
            logMessage.reserve(serializedRequest.size());
            std::remove_copy(
                serializedRequest.begin(),
                serializedRequest.end(),
                std::back_inserter(logMessage), '\r');
            Log()->trace("WsClient -> : {}", logMessage);
        }

        MessageBuffer requestMessage;
        requestMessage.assign(serializedRequest);
        send(scd, &requestMessage);
    }
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

    const std::string serializedResponse = rtsp::Serialize(*response);
    if(serializedResponse.empty()) {
        scd->data->terminateSession = true;
        lws_callback_on_writable(scd->wsi);
    } else {
        if(Log()->level() <= spdlog::level::trace) {
            std::string logMessage;
            logMessage.reserve(serializedResponse.size());
            std::remove_copy(
                serializedResponse.begin(),
                serializedResponse.end(),
                std::back_inserter(logMessage), '\r');
            Log()->trace("WsClient -> : {}", logMessage);
        }

        MessageBuffer responseMessage;
        responseMessage.assign(serializedResponse);
        send(scd, &responseMessage);
    }
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
