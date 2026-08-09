#include "WsServer.h"

#include <deque>
#include <algorithm>
#include <optional>

#include <CxxPtr/libwebsocketsPtr.h>

#include "Helpers/MessageBuffer.h"

#include "RtspParser/RtspParser.h"
#include "RtspParser/RtspSerialize.h"

#include "Common.h"
#include "Log.h"


#define SESSION "[{}]" " "


// FIXME! limit max incoming message size

// #define ENABLE_DYNAMIC_AGENTS 1

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
    std::string clientId;
    std::string agentId;

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

std::string ClientIpString(lws* wsi) noexcept
{
    char clientIp[INET6_ADDRSTRLEN];
    lws_get_peer_simple(wsi, clientIp, sizeof(clientIp));

    char xRealIp[INET6_ADDRSTRLEN];
    const bool xRealIpPresent =
        lws_hdr_copy(wsi, xRealIp, sizeof(xRealIp), WSI_TOKEN_HTTP_X_REAL_IP) > 0;

    // does not include the space for a terminating '\0'
    const int xForwardedForlength = lws_hdr_total_length(wsi, WSI_TOKEN_X_FORWARDED_FOR);
    if(xForwardedForlength > 0) {
        char xForwardedFor[xForwardedForlength + 1];
        lws_hdr_copy(wsi, xForwardedFor, sizeof(xForwardedFor), WSI_TOKEN_X_FORWARDED_FOR);
        if(xRealIpPresent) {
            return fmt::format(
                "IP: {}, X-Real-IP: {}, X-Forwarded-For: {}",
                clientIp,
                xRealIp,
                &xForwardedFor[0]);
        } else {
            return fmt::format(
                "IP: {}, X-Forwarded-For: {}",
                clientIp,
                &xForwardedFor[0]);
        }
    } else if(xRealIpPresent) {
        return fmt::format(
            "IP: {}, X-Real-IP: {}",
            clientIp,
            xRealIp);
    } else {
        return fmt::format(
            "IP: {}",
            clientIp);
    }
}

}


struct WsServer::Private
{
    Private(
        const WsServerConfig&,
        SessionFactory*,
        WsServer::AgentsDb*) noexcept;

    bool init(GMainLoop*, lws_context*) noexcept;
    int httpCallback(lws*, lws_callback_reasons, void* user, void* in, size_t len) noexcept;
    int wsCallback(lws*, lws_callback_reasons, void* user, void* in, size_t len) noexcept;
    bool onMessage(SessionContextData*, const MessageBuffer&) noexcept;

    void send(SessionContextData*, MessageBuffer*) noexcept;
    void sendRequest(SessionContextData*, const rtsp::Request*) noexcept;
    void sendResponse(SessionContextData*, const rtsp::Response*) noexcept;

    WsServerConfig config;
    SessionFactory *const sessionFactory;
    AgentsDb *const agentsDb;

    LwsContextPtr contextPtr;
};

WsServer::Private::Private(
    const WsServerConfig& config,
    WsServer::SessionFactory* sessionFactory,
    WsServer::AgentsDb* agentsDb) noexcept :
    config(config),
    sessionFactory(sessionFactory),
    agentsDb(agentsDb)
{
}

int WsServer::Private::httpCallback(
    lws* wsi,
    lws_callback_reasons reason,
    void* user, void* in, size_t len) noexcept
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
    void* in, size_t len) noexcept
{
    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            break;
        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION: {
            char valueBuffer[64 + 1];
            const bool hasClientId = lws_hdr_custom_copy(
                wsi,
                valueBuffer,
                sizeof(valueBuffer),
                webrtsp::ClientIdFieldName,
                std::string_view(webrtsp::ClientIdFieldName).size()) > 1; // > 1 since lws_hdr_custom_copy returns size including terminating zero

            if(hasClientId && !agentsDb) {
                Log()->error(
                    "Got connection from Agent without agents support. {}",
                    ClientIpString(wsi));
                return -1;
            }

            if(!agentsDb || !hasClientId)
                break;

            std::string clientId = valueBuffer;

            const bool hasAgentId = lws_hdr_custom_copy(
                wsi,
                valueBuffer,
                sizeof(valueBuffer),
                webrtsp::AgentIdFieldName,
                std::string_view(webrtsp::AgentIdFieldName).size()) > 1;
#if ENABLE_DYNAMIC_AGENTS
            if(!hasAgentId)
                break; // new agent registration. Will be finished in LWS_CALLBACK_ADD_HEADERS.
#else
            if(!hasAgentId) {
                Log()->error("Got Agent connection with only Client ID provided. Client Id: {}, {}",
                    clientId,
                    ClientIpString(wsi));

                return -1;
            }
#endif

            SessionContextData* scd = static_cast<SessionContextData*>(user);

            scd->data = new SessionData { .clientId = std::move(clientId) };

            scd->data->agentId = valueBuffer;

            const bool hasToken = lws_hdr_custom_copy(
                wsi,
                valueBuffer,
                sizeof(valueBuffer),
                webrtsp::TokenFieldName,
                std::string_view(webrtsp::TokenFieldName).size()) > 1;
            if(!hasToken) {
                Log()->error(
                    "Got Agent ID without access token. {}",
                    ClientIpString(wsi));
                return -1;
            }

            if(!agentsDb->authenticateAgent(
                scd->data->clientId,
                scd->data->agentId,
                valueBuffer))
            {
                Log()->error(
                    "Agent authentication failed. Client Id: {}, Agent Id: {}, {}",
                    scd->data->clientId,
                    scd->data->agentId,
                    ClientIpString(wsi));

                lws_return_http_status(wsi, HTTP_STATUS_UNAUTHORIZED, "Unknown Agent");
                return -1;
            }

            Log()->debug("Agent authorized. Client Id: {}, Agent Id: {}, {}",
                scd->data->clientId,
                scd->data->agentId,
                ClientIpString(wsi));

            break;
        }
        case LWS_CALLBACK_ADD_HEADERS: {
            if(!agentsDb)
                break;

            SessionContextData* scd = static_cast<SessionContextData*>(user);

            if(!scd->data || scd->data->clientId.empty())
                break; // regular client

            if(!scd->data->agentId.empty())
                break; // agent already authorized in LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION

#if ENABLE_DYNAMIC_AGENTS
            // Agent is not registered yet
            std::optional<AgentsDb::AgentCredentials> credentials =
                agentsDb->registerAgent(scd->data->clientId);
            if(!credentials.has_value()) {
                Log()->error("Failed to register new Agent. Client Id: {}, {}",
                    scd->data->clientId,
                    ClientIpString(wsi));
                return -1;
            }

            Log()->info("New Agent registered. Client Id: {}, Agent Id: {}, {}",
                scd->data->clientId,
                credentials->agentId,
                ClientIpString(wsi));

            struct lws_process_html_args* htmlArgs = reinterpret_cast<lws_process_html_args*>(in);
            unsigned char** p = reinterpret_cast<unsigned char**>(&htmlArgs->p);
            unsigned char* end = *p + htmlArgs->max_len;

            if(lws_add_http_header_by_name(
                wsi,
                reinterpret_cast<const unsigned char*>(webrtsp::AgentIdFieldName),
                reinterpret_cast<const unsigned char*>(credentials->agentId.c_str()),
                static_cast<int>(credentials->agentId.size()),
                p,
                end) != 0)
            {
                Log()->error("Failed to set \"{}\" header value", webrtsp::AgentIdFieldName);
                return -1;
            }

            if(lws_add_http_header_by_name(
                wsi,
                reinterpret_cast<const unsigned char*>(webrtsp::TokenFieldName),
                reinterpret_cast<const unsigned char*>(credentials->accessToken.c_str()),
                static_cast<int>(credentials->accessToken.size()),
                p,
                end) != 0)
            {
                Log()->error("Failed to set \"{}\" header value", webrtsp::TokenFieldName);
                return -1;
            }

            scd->data->agentId = std::move(credentials->agentId);
#endif

            break;
        }
        case LWS_CALLBACK_ESTABLISHED: {
            SessionContextData* scd = static_cast<SessionContextData*>(user);

            if(!scd->data)
                scd->data = new SessionData;

            scd->wsi = wsi;

            std::unique_ptr<rtsp::Session> session;
            if(scd->data->agentId.empty()) {
                std::optional<std::string> authCookie;
                char cookieBuf[256];
                size_t cookieSize = sizeof(cookieBuf);
                if(0 == lws_http_cookie_get(wsi, AuthCookieName, cookieBuf, &cookieSize))
                    authCookie = std::string(cookieBuf, cookieSize);

                Log()->info(
                    "Client connected. {}",
                    ClientIpString(wsi));

                session = sessionFactory->createSession(
                    std::move(authCookie),
                    [this, scd] (const rtsp::Request* request) {
                        sendRequest(scd, request);
                    },
                    [this, scd] (const rtsp::Response* response) {
                        sendResponse(scd, response);
                    });
            } else {
                Log()->info(
                    "Agent connected. Client Id: {}, Agent Id: {}, {}",
                    scd->data->clientId,
                    scd->data->agentId,
                    ClientIpString(wsi));

                session = sessionFactory->createAgentSession(
                    std::move(scd->data->clientId),
                    std::move(scd->data->agentId),
                    [this, scd] (const rtsp::Request* request) {
                        sendRequest(scd, request);
                    },
                    [this, scd] (const rtsp::Response* response) {
                        sendResponse(scd, response);
                    });
            }

            if(!session) {
                Log()->error("Failed to create session. Requesting connection close...");
                return -1;
            }

            scd->data->rtspSession = std::move(session);

            if(!scd->data->rtspSession->onConnected()) {
                Log()->error(
                    SESSION "session requested connection close in onConnected handler",
                    scd->data->rtspSession->sessionLogId);
                return -1;
            }

            break;
        }
        case LWS_CALLBACK_RECEIVE_PONG:
            Log()->trace("PONG");
            break;
        case LWS_CALLBACK_RECEIVE: {
            SessionContextData* scd = static_cast<SessionContextData*>(user);

            if(scd->data->incomingMessage.onReceive(wsi, in, len)) {
                const rtsp::Session *const session = scd->data->rtspSession.get();

                if(Log()->level() <= spdlog::level::trace) {
                    std::string logMessage;
                    logMessage.reserve(scd->data->incomingMessage.size());
                    std::remove_copy(
                        scd->data->incomingMessage.data(),
                        scd->data->incomingMessage.data() + scd->data->incomingMessage.size(),
                        std::back_inserter(logMessage), '\r');

                    Log()->trace(
                        SESSION "-> WsServer: {}",
                        session->sessionLogId,
                        logMessage);
                }

                if(!onMessage(scd, scd->data->incomingMessage)) {
                    Log()->error(
                        SESSION "session message handler requested connection close",
                        session->sessionLogId);
                    return -1;
                }

                scd->data->incomingMessage.clear();
            }

            break;
        }
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            SessionContextData* scd = static_cast<SessionContextData*>(user);

            const rtsp::Session *const session = scd->data->rtspSession.get();

            if(scd->data->terminateSession) {
                Log()->debug(
                    SESSION "session requested connection close",
                    session->sessionLogId);
                return -1;
            }

            if(!scd->data->sendMessages.empty()) {
                MessageBuffer& buffer = scd->data->sendMessages.front();
                if(!buffer.writeAsText(wsi)) {
                    Log()->error(SESSION "write failed", session->sessionLogId);
                    return -1;
                }

                scd->data->sendMessages.pop_front();

                if(!scd->data->sendMessages.empty())
                    lws_callback_on_writable(wsi);
            }

            break;
        }
        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE: {
            SessionContextData* scd = static_cast<SessionContextData*>(user);

            if(scd->data->rtspSession) {
                Log()->debug(
                    SESSION "client initiated connection close",
                    scd->data->rtspSession->sessionLogId);
            } else {
                Log()->debug("client initiated connection close");
            }

            break;
        }
        case LWS_CALLBACK_CLOSED: {
            SessionContextData* scd = static_cast<SessionContextData*>(user);

            scd->wsi = nullptr;

            if(!scd->data) // connection is filtered
                break;

            if(scd->data->rtspSession) {
                Log()->debug(
                    SESSION "connection closed",
                    scd->data->rtspSession->sessionLogId);
            }

            delete scd->data;
            scd->data = nullptr;

            break;
        }
        default:
            break;
    }

    return 0;
}

bool WsServer::Private::init(GMainLoop* loop, lws_context* context) noexcept
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
    const MessageBuffer& message) noexcept
{
    rtsp::Session *const session = scd->data->rtspSession.get();

    if(rtsp::IsRequest(message.data(), message.size())) {
        std::unique_ptr<rtsp::Request> requestPtr =
            std::make_unique<rtsp::Request>();
        if(!rtsp::ParseRequest(message.data(), message.size(), requestPtr.get())) {
            Log()->error(
                SESSION "Failed to parse request:\n{}\nForcing session disconnect...",
                session->sessionLogId,
                std::string_view(message.data(), message.size()));
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
                    SESSION "Got {} request for \"{}\"",
                    session->sessionLogId,
                    rtsp::MethodName(requestPtr->method),
                    requestPtr->uri);
                break;
        }

        if(!session->handleRequest(std::move(requestPtr))) {
            Log()->debug(
                SESSION "Failed to handle request:\n{}\nForcing session disconnect...",
                session->sessionLogId,
                std::string_view(message.data(), message.size()));
            return false;
        }
    } else {
        std::unique_ptr<rtsp::Response> responsePtr =
            std::make_unique<rtsp::Response>();
        if(!rtsp::ParseResponse(message.data(), message.size(), responsePtr.get())) {
            Log()->error(
                SESSION "Failed to parse response:\n{}\nForcing session disconnect...",
                session->sessionLogId,
                std::string_view(message.data(), message.size()));
            return false;
        }

        if(!session->handleResponse(std::move(responsePtr))) {
            Log()->error(
                SESSION "Failed to handle response:\n{}\nForcing session disconnect...",
                session->sessionLogId,
                std::string_view(message.data(), message.size()));
            return false;
        }
    }

    return true;
}

void WsServer::Private::send(
    SessionContextData* scd,
    MessageBuffer* message) noexcept
{
    scd->data->sendMessages.emplace_back(std::move(*message));

    lws_callback_on_writable(scd->wsi);
}

void WsServer::Private::sendRequest(
    SessionContextData* scd,
    const rtsp::Request* request) noexcept
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
                SESSION "WsServer -> : {}",
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
    const rtsp::Response* response) noexcept
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
                SESSION "WsServer -> : {}",
                scd->data->rtspSession->sessionLogId,
                logMessage);
        }

        MessageBuffer responseMessage;
        responseMessage.assign(serializedResponse);
        send(scd, &responseMessage);
    }
}

WsServer::WsServer(
    const WsServerConfig& config,
    SessionFactory* sessionFactory,
    AgentsDb* agentsDb) noexcept :
    _p(std::make_unique<Private>(config, sessionFactory, agentsDb))
{
}

WsServer::~WsServer() noexcept
{
}

bool WsServer::init(GMainLoop* loop, lws_context* context /*= nullptr*/) noexcept
{
    return _p->init(loop, context);
}
