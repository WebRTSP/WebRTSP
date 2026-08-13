#include "WsClient.h"

#include <deque>
#include <algorithm>

#include <CxxPtr/libwebsocketsPtr.h>

#include "Helpers/MessageBuffer.h"
#include "RtspParser/RtspSerialize.h"
#include "RtspParser/RtspParser.h"

#include "Common.h"
#include "Log.h"


#define SESSION "[{}]" " "


namespace {

enum {
    RX_BUFFER_SIZE = 512,
    PING_INTERVAL = 30,
    INCOMING_MESSAGE_WAIT_INTERVAL = PING_INTERVAL + 5,
};

#if LWS_LIBRARY_VERSION_MAJOR < 3
enum {
    LWS_CALLBACK_CLIENT_CLOSED = LWS_CALLBACK_CLOSED
};
#endif

struct SessionContextData
{
    std::string clientId;
    std::string agentId;
    std::string accessToken;

    lws* wsi = nullptr;
    bool terminateSession = false;
    MessageBuffer incomingMessage;
    std::deque<MessageBuffer> sendMessages;
    std::unique_ptr<rtsp::Session> rtspSession;
};

const auto Log = WsClientLog;

}

struct WsClient::Private final
{
    Private(
        WsClient*,
        std::string&& trustedCAs,
        const WsClientConfig&,
        SessionFactory*,
        const Disconnected&) noexcept;
    ~Private() noexcept;

    bool init(GMainLoop*, SSL_CTX*) noexcept;
    void loadTrustedCAs(SSL_CTX*) noexcept;
    int wsCallback(lws*, lws_callback_reasons, void* user, void* in, size_t len) noexcept;
    bool onMessage(const MessageBuffer&) noexcept;

    void send(MessageBuffer*) noexcept;
    void sendRequest(const rtsp::Request*) noexcept;
    void sendResponse(const rtsp::Response*) noexcept;

    void connect(const std::string& clientId,
        const std::string& agentId,
        const std::string& accessToken) noexcept;
    bool onConnected() noexcept;
    void disconnect() noexcept;

    WsClient *const owner;
    const std::string trustedCAs;
    WsClientConfig config;
    SessionFactory *const sessionFactory;
    Disconnected disconnected;

    LwsContextPtr contextPtr;

    std::unique_ptr<SessionContextData> sessionContextData;
    lws* connection = nullptr;
    bool connected = false;
};

WsClient::Private::Private(
    WsClient* owner,
    std::string&& trustedCAs,
    const WsClientConfig& config,
    SessionFactory* sessionFactory,
    const Disconnected& disconnected) noexcept :
    owner(owner), trustedCAs(std::move(trustedCAs)), config(config),
    sessionFactory(sessionFactory), disconnected(disconnected)
{
}

WsClient::Private::~Private() noexcept
{
    assert(!connection);
    if(connection) {
        lws_set_timeout(connection, NO_PENDING_TIMEOUT, LWS_TO_KILL_SYNC); // FIXME?
        connection = nullptr;
        connected = false;
        sessionContextData.reset();
    }
}

void WsClient::Private::loadTrustedCAs(SSL_CTX* sslContext) noexcept
{
    if(trustedCAs.empty())
        return;

    X509_STORE* store = SSL_CTX_get_cert_store(sslContext);
    if(!store)
        return;

    BIO* bio = BIO_new_mem_buf(trustedCAs.data(), trustedCAs.size());
    if (!bio)
        return;

    while(X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr)) {
        X509_STORE_add_cert(store, cert);
        X509_free(cert);
    }

    BIO_free(bio);
}

int WsClient::Private::wsCallback(
    lws* wsi,
    lws_callback_reasons reason,
    void* user,
    void* in, size_t len) noexcept
{
    switch(reason) {
        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS: {
            SSL_CTX* sslContext = static_cast<SSL_CTX*>(user);
            if(sslContext)
                loadTrustedCAs(sslContext);

            break;
        }
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
            SessionContextData& scd = *sessionContextData;
            if(scd.clientId.empty()) // do nothing if not in agent mode
                break;

            unsigned char** p = reinterpret_cast<unsigned char**>(in);
            unsigned char* end = *p + len;

            if(lws_add_http_header_by_name(
                wsi,
                reinterpret_cast<const unsigned char*>(webrtsp::ClientIdFieldName),
                reinterpret_cast<const unsigned char*>(scd.clientId.c_str()),
                static_cast<int>(scd.clientId.size()),
                p,
                end) != 0)
            {
                Log()->error("Failed to set \"{}\" header value", webrtsp::ClientIdFieldName);
                return -1;
            }

            if(!scd.agentId.empty() && !scd.accessToken.empty()) {
                if(lws_add_http_header_by_name(
                    wsi,
                    reinterpret_cast<const unsigned char*>(webrtsp::AgentIdFieldName),
                    reinterpret_cast<const unsigned char*>(scd.agentId.c_str()),
                    static_cast<int>(scd.agentId.size()),
                    p,
                    end) != 0)
                {
                    Log()->error("Failed to set \"{}\" header value", webrtsp::AgentIdFieldName);
                    return -1;
                }

                if(lws_add_http_header_by_name(
                    wsi,
                    reinterpret_cast<const unsigned char*>(webrtsp::TokenFieldName),
                    reinterpret_cast<const unsigned char*>(scd.accessToken.c_str()),
                    static_cast<int>(scd.accessToken.size()),
                    p,
                    end) != 0)
                {
                    Log()->error("Failed to set \"{}\" header value", webrtsp::TokenFieldName);
                    return -1;
                }
            }

            break;
        }
        case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH: {
            SessionContextData& scd = *sessionContextData;
            if(scd.clientId.empty()) // do nothing if not in agent mode
                break;

            char valueBuffer[64 + 1];

            std::string agentId;
            const bool hasAgentId = lws_hdr_custom_copy(
                wsi,
                valueBuffer,
                sizeof(valueBuffer),
                webrtsp::AgentIdFieldName,
                std::string_view(webrtsp::AgentIdFieldName).size()) > 1;
            if(!hasAgentId && scd.agentId.empty()) {
                Log()->error("Didn't get Agent ID from server");
                return -1;
            }
            if(hasAgentId)  {
                if(!scd.agentId.empty() && valueBuffer != scd.agentId) {
                    Log()->error("Got wrong Agent ID from server");
                    return -1;
                }
                agentId = valueBuffer;
            }

            if(!agentId.empty()) {
                const bool hasToken = lws_hdr_custom_copy(
                    wsi,
                    valueBuffer,
                    sizeof(valueBuffer),
                    webrtsp::TokenFieldName,
                    std::string_view(webrtsp::TokenFieldName).size()) > 1;
                if(!hasToken) {
                    Log()->error("Didn't get token from server");
                    return -1;
                }

                scd.agentId = agentId;
                scd.accessToken = valueBuffer;
            }
            assert(!scd.agentId.empty() && !scd.accessToken.empty());

            break;
        }
        case LWS_CALLBACK_CLIENT_ESTABLISHED: {
            SessionContextData& scd = *sessionContextData;

            if(scd.terminateSession) {
                Log()->info("Requested disconnect before connect.");
                return -1;
            }

            std::unique_ptr<rtsp::Session> session;
            if(scd.agentId.empty()) {
                Log()->info("Connected to server");

                session = sessionFactory->createSession(
                    [this] (const rtsp::Request* request) { sendRequest(request); },
                    [this] (const rtsp::Response* response) { sendResponse(response); });
            } else {
                Log()->info(
                    "Connected to server as agent. Client Id: {}, Agent Id: {}",
                    scd.clientId,
                    scd.agentId);

                session = sessionFactory->createAgentSession(
                    std::string(std::move(scd.clientId)), // clientId is never used after
                    std::move(scd.agentId),
                    std::move(scd.accessToken),
                    [this] (const rtsp::Request* request) { sendRequest(request); },
                    [this] (const rtsp::Response* response) { sendResponse(response); });
            }

            if(!session) {
                Log()->error("Failed to create session. Requesting connection close...");
                return -1;
            }

            scd.wsi = wsi;
            scd.rtspSession = std::move(session);

            connected = true;

            if(!onConnected()) {
                Log()->error(
                    SESSION "Session requested connection close in onConnected handler",
                    scd.rtspSession->sessionLogId);
                return -1;
            }

            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            Log()->trace("PONG");
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            SessionContextData& scd = *sessionContextData;

            if(scd.incomingMessage.onReceive(wsi, in, len)) {
                if(Log()->level() <= spdlog::level::trace) {
                    std::string logMessage;
                    logMessage.reserve(scd.incomingMessage.size());
                    std::remove_copy(
                        scd.incomingMessage.data(),
                        scd.incomingMessage.data() + scd.incomingMessage.size(),
                        std::back_inserter(logMessage), '\r');

                    Log()->trace(
                        SESSION "-> WsClient: {}",
                        scd.rtspSession->sessionLogId,
                        logMessage);
                }

                if(!onMessage(scd.incomingMessage)) {
                    Log()->error(
                        SESSION "message handler requested connection close",
                        scd.rtspSession->sessionLogId);
                    return -1;
                }

                scd.incomingMessage.clear();
            }

            break;
        }
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            SessionContextData& scd = *sessionContextData;

            if(scd.terminateSession)
                return -1;

            if(!scd.sendMessages.empty()) {
                MessageBuffer& buffer = scd.sendMessages.front();
                if(!buffer.writeAsText(wsi)) {
                    Log()->error(
                        SESSION "Write failed.",
                        scd.rtspSession->sessionLogId);
                    return -1;
                }

                scd.sendMessages.pop_front();

                if(!scd.sendMessages.empty())
                    lws_callback_on_writable(wsi);
            }

            break;
        }
        case LWS_CALLBACK_CLIENT_CLOSED:
            Log()->info("Connection to server is closed.");

            sessionContextData.reset();
            connection = nullptr;
            connected = false;

            if(disconnected)
                disconnected(*this->owner);

            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            Log()->error("Can not connect to server.");

            sessionContextData.reset();
            connection = nullptr;
            connected = false;

            if(disconnected)
                disconnected(*this->owner);

            break;
        default:
            break;
    }

    return 0;
}

bool WsClient::Private::init(GMainLoop* loop, SSL_CTX* sslContext) noexcept
{
    auto WsCallback =
        [] (lws* wsi, lws_callback_reasons reason, void* user, void* in, size_t len) -> int {
            lws_context* context = lws_get_context(wsi);
            Private* p = static_cast<Private*>(lws_context_user(context));
            return p->wsCallback(wsi, reason, user, in, len);
        };

    static const lws_protocols protocols[] = {
        {
            .name = "webrtsp",
            .callback = WsCallback,
            .rx_buffer_size = RX_BUFFER_SIZE,
        },
        LWS_PROTOCOL_LIST_TERM
    };

#if LWS_LIBRARY_VERSION_NUMBER >= 4000000
    lws_retry_bo_t retryPolicy {
        .secs_since_valid_ping = PING_INTERVAL,
        .secs_since_valid_hangup = INCOMING_MESSAGE_WAIT_INTERVAL,
    };
#endif
    lws_context_creation_info wsInfo {
        .protocols = protocols,
        .port = CONTEXT_PORT_NO_LISTEN,
        .provided_client_ssl_ctx = sslContext,
        .gid = gid_t(-1),
        .uid = uid_t(-1),
        .options = LWS_SERVER_OPTION_GLIB,
        .user = this,
        .foreign_loops = reinterpret_cast<void**>(&loop),
#if LWS_LIBRARY_VERSION_NUMBER < 4000000
        .ws_ping_pong_interval = PING_INTERVAL,
#else
        .retry_and_idle_policy = &retryPolicy,
#endif
    };

    if(sslContext)
        loadTrustedCAs(sslContext);
    else
        wsInfo.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    contextPtr.reset(lws_create_context(&wsInfo));
    lws_context* context = contextPtr.get();
    if(!context)
        return false;

    return true;
}

void WsClient::Private::connect(
    const std::string& clientId,
    const std::string& agentId,
    const std::string& accessToken) noexcept
{
    if(connection)
        return;

    if(config.server.empty() || !config.serverPort) {
        Log()->error("Missing required connect parameter.");
        return;
    }

    Log()->info("Connecting to {}:{}...", config.server, config.serverPort);

    assert(!sessionContextData);
    sessionContextData = std::make_unique<SessionContextData>(
        clientId,
        agentId,
        accessToken);
    struct lws_client_connect_info connectInfo = {
        .context = contextPtr.get(),
        .address = config.server.c_str(),
        .port = config.serverPort,
        .ssl_connection = config.useTls ? LCCSCF_USE_SSL : 0,
        .path = "/",
        .host = config.server.c_str(),
        .protocol = "webrtsp",
    };
    connection = lws_client_connect_via_info(&connectInfo);
    connected = false;
}

bool WsClient::Private::onConnected() noexcept
{
    return sessionContextData->rtspSession->onConnected();
}

void WsClient::Private::disconnect() noexcept
{
    if(!connection)
        return;

    assert(sessionContextData);

    SessionContextData& scd = *sessionContextData;
    scd.terminateSession = true;
    lws_callback_on_writable(scd.wsi);
}

bool WsClient::Private::onMessage(const MessageBuffer& message) noexcept
{
    SessionContextData& scd = *sessionContextData;

    if(rtsp::IsRequest(message.data(), message.size())) {
        std::unique_ptr<rtsp::Request> requestPtr =
            std::make_unique<rtsp::Request>();
        if(!rtsp::ParseRequest(message.data(), message.size(), requestPtr.get())) {
            Log()->error(
                SESSION "Failed to parse request:\n{}\nForcing session disconnect...",
                scd.rtspSession->sessionLogId,
                std::string_view(message.data(), message.size()));
            return false;
        }

        if(!scd.rtspSession->handleRequest(std::move(requestPtr))) {
            scd.rtspSession->log()->debug(
                SESSION "Failed to handle request:\n{}\nForcing session disconnect...",
                scd.rtspSession->sessionLogId,
                std::string_view(message.data(), message.size()));
            return false;
        }
    } else {
        std::unique_ptr<rtsp::Response > responsePtr =
            std::make_unique<rtsp::Response>();
        if(!rtsp::ParseResponse(message.data(), message.size(), responsePtr.get())) {
            Log()->error(
                SESSION "Failed to parse response:\n{}\nForcing session disconnect...",
                scd.rtspSession->sessionLogId,
                std::string_view(message.data(), message.size()));
            return false;
        }

        if(!scd.rtspSession->handleResponse(std::move(responsePtr))) {
            scd.rtspSession->log()->error(
                SESSION "Failed to handle response:\n{}\nForcing session disconnect...",
                scd.rtspSession->sessionLogId,
                std::string_view(message.data(), message.size()));
            return false;
        }
    }

    return true;
}

void WsClient::Private::send(MessageBuffer* message) noexcept
{
    assert(!message->empty());

    SessionContextData& scd = *sessionContextData;

    scd.sendMessages.emplace_back(std::move(*message));

    lws_callback_on_writable(scd.wsi);
}

void WsClient::Private::sendRequest(const rtsp::Request* request) noexcept
{
    if(!request) {
        disconnect();
        return;
    }

    if(!sessionContextData || !connection || !connected || sessionContextData->terminateSession) {
        Log()->error("sendRequest called in invalid internal state");
        return;
    }

    SessionContextData& scd = *sessionContextData;

    const std::string serializedRequest = rtsp::Serialize(*request);
    if(serializedRequest.empty()) {
        scd.terminateSession = true;
        lws_callback_on_writable(scd.wsi);
    } else {
        if(Log()->level() <= spdlog::level::trace) {
            std::string logMessage;
            logMessage.reserve(serializedRequest.size());
            std::remove_copy(
                serializedRequest.begin(),
                serializedRequest.end(),
                std::back_inserter(logMessage), '\r');
            Log()->trace(
                SESSION "WsClient -> : {}",
                scd.rtspSession->sessionLogId,
                logMessage);
        }

        MessageBuffer requestMessage;
        requestMessage.assign(serializedRequest);
        send(&requestMessage);
    }
}

void WsClient::Private::sendResponse(const rtsp::Response* response) noexcept
{
    if(!response) {
        disconnect();
        return;
    }

    if(!sessionContextData || !connection || !connected || sessionContextData->terminateSession) {
        Log()->error("sendResponse called in invalid internal state");
        return;
    }

    SessionContextData& scd = *sessionContextData;

    const std::string serializedResponse = rtsp::Serialize(*response);
    if(serializedResponse.empty()) {
        scd.terminateSession = true;
        lws_callback_on_writable(scd.wsi);
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
        send(&responseMessage);
    }
}

WsClient::WsClient(
    std::string&& trustedCAs,
    const WsClientConfig& config,
    SessionFactory* sessionFactory,
    const Disconnected& disconnected) noexcept :
    _p(std::make_unique<Private>(
        this,
        std::move(trustedCAs),
        config,
        sessionFactory,
        disconnected))
{
}

WsClient::~WsClient() noexcept
{
}

bool WsClient::init(GMainLoop* loop, SSL_CTX* sslCtx) noexcept
{
    return _p->init(loop, sslCtx);
}

void WsClient::connect() noexcept
{
    _p->connect({}, {}, {});
}

void WsClient::connectAsAgent(
    const std::string& clientId,
    const std::string& agentId,
    const std::string& accessToken) noexcept
{
    _p->connect(clientId, agentId, accessToken);
}

void WsClient::disconnect() noexcept
{
    _p->disconnect();
}
