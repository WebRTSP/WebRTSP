#include "Signalling.h"

#include <deque>

#include <libwebsockets.h>

#include <CxxPtr/libwebsocketsPtr.h>
#include <CxxPtr/GlibPtr.h>

#include "Common/MessageBuffer.h"
#include "Common/LwsSource.h"
#include "RtspParser/RtspParser.h"
#include "RtspParser/RtspSerialize.h"
#include "ServerSession.h"


namespace signalling {

enum {
    RX_BUFFER_SIZE = 512,
};

enum {
    HTTP_PROTOCOL_ID,
    PROTOCOL_ID,
    HTTPS_PROTOCOL_ID,
    SECURE_PROTOCOL_ID,
};

namespace {

struct ContextData
{
    LwsSourcePtr lwsSource;
};

struct SessionData
{
    bool terminateSession = false;
    MessageBuffer incomingMessage;
    std::deque<MessageBuffer> sendMessages;
    ServerSession rtspSession;
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

int HttpCallback(
    lws* wsi,
    lws_callback_reasons reason,
    void* user,
    void* in, size_t len)
{
    lws_context* context = lws_get_context(wsi);
    ContextData* cd = static_cast<ContextData*>(lws_context_user(context));
    switch(reason) {
        case LWS_CALLBACK_ADD_POLL_FD:
        case LWS_CALLBACK_DEL_POLL_FD:
        case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
            return LwsSourceCallback(cd->lwsSource, wsi, reason, in, len);
        default:
            return lws_callback_http_dummy(wsi, reason, user, in, len);
    }

    return 0;
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
        case LWS_CALLBACK_PROTOCOL_INIT:
            break;
        case LWS_CALLBACK_ESTABLISHED: {
            scd->data =
                new SessionData {
                    .incomingMessage ={},
                    .sendMessages = {},
                    .rtspSession = {
                        std::bind(SendRequest, cd, scd, std::placeholders::_1),
                        std::bind(SendResponse, cd, scd, std::placeholders::_1) }
                };
            scd->wsi = wsi;
            break;
        }
        case LWS_CALLBACK_RECEIVE: {
            if(scd->data->incomingMessage.onReceive(wsi, in, len)) {
                lwsl_notice("-> Signalling: %.*s\n", static_cast<int>(scd->data->incomingMessage.size()), scd->data->incomingMessage.data());

                if(!OnMessage(cd, scd, scd->data->incomingMessage))
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
                    lwsl_err("write failed\n");
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

bool Signalling(Config* config) noexcept
{
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
        { nullptr, nullptr, 0, 0 } /* terminator */
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
        { nullptr, nullptr, 0, 0 } /* terminator */
    };

    GMainLoopPtr loopPtr(g_main_loop_new(nullptr, FALSE));
    GMainLoop* loop = loopPtr.get();

    ContextData contextData {};

    lws_context_creation_info wsInfo {};
    wsInfo.gid = -1;
    wsInfo.uid = -1;
    wsInfo.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
    wsInfo.user = &contextData;

    LwsContextPtr contextPtr(lws_create_context(&wsInfo));
    lws_context* context = contextPtr.get();
    if(!context)
        return false;

    contextData.lwsSource = LwsSourceNew(context);
    if(!contextData.lwsSource)
        return false;

    bool run = false;

    if(config->port != 0) {
        lws_context_creation_info vhostInfo {};
        vhostInfo.port = config->port;
        vhostInfo.protocols = protocols;

        lws_vhost* vhost = lws_create_vhost(context, &vhostInfo);
        if(!vhost)
             return false;

        run = true;
    }

    if(!config->serverName.empty() && config->securePort != 0 &&
        !config->certificate.empty() && !config->key.empty())
    {
        lws_context_creation_info secureVhostInfo {};
        secureVhostInfo.port = config->securePort;
        secureVhostInfo.protocols = secureProtocols;
        secureVhostInfo.ssl_cert_filepath = config->certificate.c_str();
        secureVhostInfo.ssl_private_key_filepath = config->key.c_str();
        secureVhostInfo.vhost_name = config->serverName.c_str();
        secureVhostInfo.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

        lws_vhost* secureVhost = lws_create_vhost(context, &secureVhostInfo);
        if(!secureVhost)
             return false;

        run = false;
    }

    if(run) {
        g_main_loop_run(loop);

        return true;
    } else
        return false;
}

}
