#include "WsServer.h"

#include <deque>
#include <algorithm>

#include <CxxPtr/libwebsocketsPtr.h>

#include "Common/LwsSource.h"
#include "Common/MessageBuffer.h"

#include "RtspParser/RtspParser.h"
#include "RtspParser/RtspSerialize.h"

#include "Log.h"


namespace signalling {

namespace {

enum {
    RX_BUFFER_SIZE = 512,
    PING_INTERVAL = 30,
};

enum {
    HTTP_PROTOCOL_ID,
    PROTOCOL_ID,
    HTTPS_PROTOCOL_ID,
    SECURE_PROTOCOL_ID,
};

struct SessionData
{
    bool terminateSession = false;
    MessageBuffer incomingMessage;
    std::deque<MessageBuffer> sendMessages;
    std::unique_ptr<rtsp::Session> rtspSession;
};

// Should contain only POD types,
// since created inside libwebsockets on session create.
struct SessionContextData
{
    lws* wsi;
    SessionData* data;
};

const auto Log = WsServerLog;

}


struct WsServer::Private
{
    Private(WsServer*, const Config&, GMainLoop*, const WsServer::CreateSession&);

    bool init(lws_context* context);
    int httpCallback(lws*, lws_callback_reasons, void* user, void* in, size_t len);
    int wsCallback(lws*, lws_callback_reasons, void* user, void* in, size_t len);
    bool onMessage(SessionContextData*, const MessageBuffer&);

    void send(SessionContextData*, MessageBuffer*);
    void sendRequest(SessionContextData*, const rtsp::Request*);
    void sendResponse(SessionContextData*, const rtsp::Response*);

    bool onConnected(SessionContextData*);

    WsServer *const owner;
    Config config;
    GMainLoop* loop;
    CreateSession createSession;

#if !defined(LWS_WITH_GLIB)
    LwsSourcePtr lwsSourcePtr;
#endif
    LwsContextPtr contextPtr;
};

WsServer::Private::Private(
    WsServer* owner,
    const Config& config,
    GMainLoop* loop,
    const WsServer::CreateSession& createSession) :
    owner(owner), config(config), loop(loop), createSession(createSession)
{
}

int WsServer::Private::httpCallback(
    lws* wsi,
    lws_callback_reasons reason,
    void* user, void* in, size_t len)
{
    switch(reason) {
#if !defined(LWS_WITH_GLIB)
        case LWS_CALLBACK_ADD_POLL_FD:
        case LWS_CALLBACK_DEL_POLL_FD:
        case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
            return LwsSourceCallback(lwsSourcePtr, wsi, reason, in, len);
#endif
        default:
            return lws_callback_http_dummy(wsi, reason, user, in, len);
    }

    return 0;
}

int WsServer::Private::wsCallback(
    lws* wsi,
    lws_callback_reasons reason,
    void* user,
    void* in, size_t len)
{
    SessionContextData* scd = static_cast<SessionContextData*>(user);

    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            break;
        case LWS_CALLBACK_ESTABLISHED: {
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

            if(!onConnected(scd))
                return -1;

            break;
        }
        case LWS_CALLBACK_RECEIVE_PONG:
            Log()->trace("PONG");
            break;
        case LWS_CALLBACK_RECEIVE: {
            if(scd->data->incomingMessage.onReceive(wsi, in, len)) {
                if(Log()->level() <= spdlog::level::trace) {
                    std::string logMessage;
                    logMessage.reserve(scd->data->incomingMessage.size());
                    std::remove_copy(
                        scd->data->incomingMessage.data(),
                        scd->data->incomingMessage.data() + scd->data->incomingMessage.size(),
                        std::back_inserter(logMessage), '\r');

                    Log()->trace("-> WsServer: {}", logMessage);
                }

                if(!onMessage(scd, scd->data->incomingMessage))
                    return -1;

                scd->data->incomingMessage.clear();
            }

            break;
        }
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            if(scd->data->terminateSession)
                return -1;

            if(!scd->data->sendMessages.empty()) {
                MessageBuffer& buffer = scd->data->sendMessages.front();
                if(!buffer.writeAsText(wsi)) {
                    Log()->error("write failed.");
                    return -1;
                }

                scd->data->sendMessages.pop_front();

                if(!scd->data->sendMessages.empty())
                    lws_callback_on_writable(wsi);
            }

            break;
        }
        case LWS_CALLBACK_CLOSED: {
            delete scd->data;
            scd->data = nullptr;

            scd->wsi = nullptr;

            break;
        }
        default:
            break;
    }

    return 0;
}

bool WsServer::Private::init(lws_context* context)
{
    auto HttpCallback =
        [] (lws* wsi, lws_callback_reasons reason, void* user, void* in, size_t len) -> int {
            lws_vhost* vhost = lws_get_vhost(wsi);
            Private* p = static_cast<Private*>(lws_get_vhost_user(vhost));

            return p->httpCallback(wsi, reason, user, in, len);
        };
    auto WsCallback =
        [] (lws* wsi, lws_callback_reasons reason, void* user, void* in, size_t len) -> int {
            lws_vhost* vhost = lws_get_vhost(wsi);
            Private* p = static_cast<Private*>(lws_get_vhost_user(vhost));

            return p->wsCallback(wsi, reason, user, in, len);
        };

    const lws_protocols protocols[] = {
        { "http", HttpCallback, 0, 0, HTTP_PROTOCOL_ID },
        {
            "webrtsp",
            WsCallback,
            sizeof(SessionContextData),
            RX_BUFFER_SIZE,
            PROTOCOL_ID,
            nullptr
        },
        { nullptr, nullptr, 0, 0 }
    };

    const lws_protocols secureProtocols[] = {
        { "http", HttpCallback, 0, 0, HTTPS_PROTOCOL_ID },
        {
            "webrtsp",
            WsCallback,
            sizeof(SessionContextData),
            RX_BUFFER_SIZE,
            SECURE_PROTOCOL_ID,
            nullptr
        },
        { nullptr, nullptr, 0, 0 }
    };

    if(!context) {
        lws_context_creation_info wsInfo {};
        wsInfo.gid = -1;
        wsInfo.uid = -1;
#if LWS_LIBRARY_VERSION_NUMBER < 4000000
        wsInfo.ws_ping_pong_interval = PING_INTERVAL;
#else
        lws_retry_bo_t retryPolicy {};
        retryPolicy.secs_since_valid_ping = PING_INTERVAL;
        wsInfo.retry_and_idle_policy = &retryPolicy;
#endif
        wsInfo.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
#if defined(LWS_WITH_GLIB)
        wsInfo.options |= LWS_SERVER_OPTION_GLIB;
        wsInfo.foreign_loops = reinterpret_cast<void**>(&loop);
#endif

        contextPtr.reset(lws_create_context(&wsInfo));
        context = contextPtr.get();
    }
    if(!context)
        return false;

#if !defined(LWS_WITH_GLIB)
    lwsSourcePtr = LwsSourceNew(context, g_main_context_get_thread_default());
    if(!lwsSourcePtr)
        return false;
#endif

    if(config.port != 0) {
        Log()->info("Starting WS server on port {}", config.port);

        lws_context_creation_info vhostInfo {};
        vhostInfo.port = config.port;
        vhostInfo.protocols = protocols;
        vhostInfo.user = this;
        if(config.bindToLoopbackOnly)
            vhostInfo.iface = "lo";

        lws_vhost* vhost = lws_create_vhost(context, &vhostInfo);
        if(!vhost)
             return false;
    }

    if(!config.serverName.empty() && config.securePort != 0 &&
        !config.certificate.empty() && !config.key.empty())
    {
        Log()->info("Starting WSS server on port {}", config.securePort);

        lws_context_creation_info secureVhostInfo {};
        secureVhostInfo.port = config.securePort;
        secureVhostInfo.protocols = secureProtocols;
        secureVhostInfo.ssl_cert_filepath = config.certificate.c_str();
        secureVhostInfo.ssl_private_key_filepath = config.key.c_str();
        secureVhostInfo.vhost_name = config.serverName.c_str();
        secureVhostInfo.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        secureVhostInfo.user = this;
        if(config.secureBindToLoopbackOnly)
            secureVhostInfo.iface = "lo";

        lws_vhost* secureVhost = lws_create_vhost(context, &secureVhostInfo);
        if(!secureVhost)
             return false;
    }

    return true;
}

bool WsServer::Private::onConnected(SessionContextData* scd)
{
    return scd->data->rtspSession->onConnected();
}

bool WsServer::Private::onMessage(
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
        std::unique_ptr<rtsp::Response> responsePtr =
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

void WsServer::Private::send(SessionContextData* scd, MessageBuffer* message)
{
    scd->data->sendMessages.emplace_back(std::move(*message));

    lws_callback_on_writable(scd->wsi);
}

void WsServer::Private::sendRequest(
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
            Log()->trace("WsServer -> : {}", logMessage);
        }

        MessageBuffer requestMessage;
        requestMessage.assign(serializedRequest);
        send(scd, &requestMessage);
    }
}

void WsServer::Private::sendResponse(
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
            Log()->trace("WsServer -> : {}", logMessage);
        }

        MessageBuffer responseMessage;
        responseMessage.assign(serializedResponse);
        send(scd, &responseMessage);
    }
}

WsServer::WsServer(
    const Config& config,
    GMainLoop* loop,
    const CreateSession& createSession) noexcept :
    _p(std::make_unique<Private>(this, config, loop, createSession))
{
}

WsServer::~WsServer()
{
}

bool WsServer::init(lws_context* context /*= nullptr*/) noexcept
{
    return _p->init(context);
}

}
