#include "Signalling.h"

#include <deque>

#include <libwebsockets.h>

#include <CxxPtr/libwebsocketsPtr.h>

#include "Common/MessageBuffer.h"
#include "RtspParser/RtspParser.h"
#include "RtspParser/RtspSerialize.h"
#include "RtspSession.h"


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
};

struct SessionData
{
    bool terminateSession = false;
    MessageBuffer incomingMessage;
    std::deque<MessageBuffer> sendMessages;
    RtspSession rtspSession;
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

    Send(scd, &responseMessage);
}

static bool OnMessage(ContextData* cd, SessionContextData* scd, const MessageBuffer& message)
{
    rtsp::Request request;
    if(!rtsp::ParseRequest(message.data(), message.size(), &request))
        return false;

    if(!scd->data->rtspSession.handleRequest(request))
        return false;

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
        case LWS_CALLBACK_PROTOCOL_INIT:
            break;
        case LWS_CALLBACK_ESTABLISHED: {
            scd->data =
                new SessionData {
                    .incomingMessage ={},
                    .sendMessages = {},
                    .rtspSession = { std::bind(SendResponse, cd, scd, std::placeholders::_1) }
                };
            scd->wsi = wsi;
            break;
        }
        case LWS_CALLBACK_RECEIVE: {
            if(scd->data->incomingMessage.onReceive(wsi, in, len)) {
                lwsl_notice("%.*s\n", static_cast<int>(scd->data->incomingMessage.size()), scd->data->incomingMessage.data());

                OnMessage(cd, scd, scd->data->incomingMessage);

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
        { "http", lws_callback_http_dummy, 0, 0, HTTP_PROTOCOL_ID },
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
        { "http", lws_callback_http_dummy, 0, 0, HTTPS_PROTOCOL_ID },
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
        while(lws_service(context, 50) >= 0);

        return true;
    } else
        return false;
}

}
