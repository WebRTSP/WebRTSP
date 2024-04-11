#include "WsServer.h"

#include <deque>
#include <algorithm>
#include <optional>

#include <CxxPtr/libwebsocketsPtr.h>

#include "Helpers/MessageBuffer.h"

#include "RtspParser/RtspParser.h"
#include "RtspParser/RtspSerialize.h"

#include "Log.h"


namespace signalling {

namespace {

enum {
    RX_BUFFER_SIZE = 512,
    PING_INTERVAL = 2 * 60,
    INCOMING_MESSAGE_WAIT_INTERVAL = PING_INTERVAL + 30,
};

enum {
    HTTP_PROTOCOL_ID,
    PROTOCOL_ID,
};

const char* AuthCookieName = "WebRTSP-Auth";

struct SessionData
{
    bool terminateSession = false;
    MessageBuffer incomingMessage;
    std::deque<MessageBuffer> sendMessages;
    std::unique_ptr<ServerSession> rtspSession;
};

// Should contain only POD types,
// since created inside libwebsockets on session create.
struct SessionContextData
{
    lws* wsi;
    SessionData* data;
};

const auto Log = WsServerLog;

void LogClientIp(lws* wsi, const std::string& sessionLogId) {
    char clientIp[INET6_ADDRSTRLEN];
    lws_get_peer_simple(wsi, clientIp, sizeof(clientIp));

    char xRealIp[INET6_ADDRSTRLEN];
    const bool xRealIpPresent =
        lws_hdr_copy(wsi, xRealIp, sizeof(xRealIp), WSI_TOKEN_HTTP_X_REAL_IP) > 0;

    const int xForwardedForlength = lws_hdr_total_length(wsi, WSI_TOKEN_X_FORWARDED_FOR);
    if(xForwardedForlength > 0) {
        char xForwardedFor[xForwardedForlength + 1];
        lws_hdr_copy(wsi, xForwardedFor, sizeof(xForwardedFor), WSI_TOKEN_X_FORWARDED_FOR);
        if(xRealIpPresent) {
            Log()->info(
                "[{}] New client connected. IP: {}, X-Real-IP: {}, X-Forwarded-For: {}",
                sessionLogId,
                clientIp,
                xRealIp,
                &xForwardedFor[0]);
        } else {
            Log()->info(
                "[{}] New client connected. IP: {}, X-Forwarded-For: {}",
                sessionLogId,
                clientIp,
                &xForwardedFor[0]);
        }
    } else if(xRealIpPresent) {
        Log()->info(
            "[{}] New client connected. IP: {}, X-Real-IP: {}",
            sessionLogId,
            clientIp,
            xRealIp);
    } else {
        Log()->info(
            "[{}] New client connected. IP: {}",
            sessionLogId,
            clientIp);
    }
}

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

    WsServer *const owner;
    Config config;
    GMainLoop* loop;
    CreateSession createSession;

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
            std::unique_ptr<ServerSession> session =
                createSession(
                    std::bind(&Private::sendRequest, this, scd, std::placeholders::_1),
                    std::bind(&Private::sendResponse, this, scd, std::placeholders::_1));
            if(!session)
                return -1;

            LogClientIp(wsi, session->sessionLogId);

            scd->data =
                new SessionData {
                    .terminateSession = false,
                    .incomingMessage ={},
                    .sendMessages = {},
                    .rtspSession = std::move(session)};
            scd->wsi = wsi;

            std::optional<std::string> authCookie;
            char cookieBuf[256];
            size_t cookieSize = sizeof(cookieBuf);
            if(0 == lws_http_cookie_get(wsi, AuthCookieName, cookieBuf, &cookieSize)) {
                authCookie = std::string(cookieBuf, cookieSize);
            }

            if(!scd->data->rtspSession->onConnected(authCookie))
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

                    Log()->trace(
                        "[{}] -> WsServer: {}",
                        scd->data->rtspSession->sessionLogId,
                        logMessage);
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
                    Log()->error(
                        "[{}] write failed.",
                        scd->data->rtspSession->sessionLogId);
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

    if(!context) {
        lws_context_creation_info wsInfo {};
        wsInfo.gid = -1;
        wsInfo.uid = -1;
#if LWS_LIBRARY_VERSION_NUMBER < 4000000
        wsInfo.ws_ping_pong_interval = PING_INTERVAL;
#else
        lws_retry_bo_t retryPolicy {};
        retryPolicy.secs_since_valid_ping = PING_INTERVAL;
        retryPolicy.secs_since_valid_hangup = INCOMING_MESSAGE_WAIT_INTERVAL;
        wsInfo.retry_and_idle_policy = &retryPolicy;
#endif
        wsInfo.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
        wsInfo.options |= LWS_SERVER_OPTION_GLIB;
        wsInfo.foreign_loops = reinterpret_cast<void**>(&loop);

        contextPtr.reset(lws_create_context(&wsInfo));
        context = contextPtr.get();
    }
    if(!context)
        return false;

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

    return true;
}

bool WsServer::Private::onMessage(
    SessionContextData* scd,
    const MessageBuffer& message)
{
    if(rtsp::IsRequest(message.data(), message.size())) {
        std::unique_ptr<rtsp::Request> requestPtr =
            std::make_unique<rtsp::Request>();
        if(!rtsp::ParseRequest(message.data(), message.size(), requestPtr.get())) {
            Log()->error(
                "[{}] Fail parse request:\n{}\nForcing session disconnect...",
                scd->data->rtspSession->sessionLogId,
                std::string(message.data(), message.size()));
            return false;
        }

        switch(requestPtr->method) {
            case rtsp::Method::NONE:
            case rtsp::Method::OPTIONS:
            case rtsp::Method::LIST:
            case rtsp::Method::SETUP:
            case rtsp::Method::GET_PARAMETER:
            case rtsp::Method::SET_PARAMETER:
                break;
            case rtsp::Method::DESCRIBE:
            case rtsp::Method::PLAY:
            case rtsp::Method::RECORD:
            case rtsp::Method::SUBSCRIBE:
            case rtsp::Method::TEARDOWN:
                Log()->info(
                    "[{}] Got {} request for \"{}\"",
                    scd->data->rtspSession->sessionLogId,
                    rtsp::MethodName(requestPtr->method), requestPtr->uri);
                break;
        }

        if(!scd->data->rtspSession->handleRequest(requestPtr)) {
            Log()->debug(
                "[{}] Fail handle request:\n{}\nForcing session disconnect...",
                scd->data->rtspSession->sessionLogId,
                std::string(message.data(), message.size()));
            return false;
        }
    } else {
        std::unique_ptr<rtsp::Response> responsePtr =
            std::make_unique<rtsp::Response>();
        if(!rtsp::ParseResponse(message.data(), message.size(), responsePtr.get())) {
            Log()->error(
                "[{}] Fail parse response:\n{}\nForcing session disconnect...",
                scd->data->rtspSession->sessionLogId,
                std::string(message.data(), message.size()));
            return false;
        }

        if(!scd->data->rtspSession->handleResponse(responsePtr)) {
            Log()->error(
                "[{}] Fail handle response:\n{}\nForcing session disconnect...",
                scd->data->rtspSession->sessionLogId,
                std::string(message.data(), message.size()));
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
            Log()->trace(
                "[{}] WsServer -> : {}",
                scd->data->rtspSession->sessionLogId,
                logMessage);
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
            Log()->trace(
                "[{}] WsServer -> : {}",
                scd->data->rtspSession->sessionLogId,
                logMessage);
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
