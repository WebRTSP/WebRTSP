#include "HttpServer.h"

#include <deque>
#include <algorithm>

#include <CxxPtr/libwebsocketsPtr.h>

#include "Common/LwsSource.h"

#include "Log.h"


namespace http {

namespace {

const auto Log = HttpServerLog;

}


struct Server::Private
{
    Private(Server*, const Config&, unsigned short webRTSPPort, GMainLoop*);

    bool init(lws_context* context);

    int httpCallback(lws*, lws_callback_reasons, void* user, void* in, size_t len);

    Server *const owner;
    Config config;
    GMainLoop* loop;

#if !defined(LWS_WITH_GLIB)
    LwsSourcePtr lwsSourcePtr;
#endif
    LwsContextPtr contextPtr;

    std::vector<uint8_t> configJsBuffer;
};

Server::Private::Private(
    Server* owner,
    const Config& config,
    unsigned short webRTSPPort,
    GMainLoop* loop) :
    owner(owner),
    config(config),
    loop(loop)
{
    const std::string configJs =
        fmt::format("const WebRTSPPort = {} \r\n", webRTSPPort);
    configJsBuffer.reserve(configJs.size());
    configJsBuffer.insert(configJsBuffer.end(), configJs.begin(), configJs.end());
}

int Server::Private::httpCallback(
    lws* wsi,
    lws_callback_reasons reason,
    void* user,
    void* in,
    size_t len)
{
    switch(reason) {
        case LWS_CALLBACK_HTTP: {
            uint8_t headers[LWS_RECOMMENDED_MIN_HEADER_SPACE];
            uint8_t* pos = headers;
            if(
                lws_add_http_common_headers(
                    wsi, HTTP_STATUS_OK, "text/javascript",
                    configJsBuffer.size(),
                    &pos, headers + sizeof(headers) - 1) != 0 ||
                lws_finalize_write_http_header(
                    wsi,
                    headers, &pos, headers + sizeof(headers) - 1) != 0)
            {
                return 1;
            }

            lws_callback_on_writable(wsi);
            return 0;
        }
        case LWS_CALLBACK_HTTP_WRITEABLE: {
            const int written =
                lws_write(
                    wsi,
                    configJsBuffer.data(),
                    configJsBuffer.size(),
                    LWS_WRITE_HTTP_FINAL);
            if(written < 0 || static_cast<size_t>(written) != configJsBuffer.size())
                return 1;

            if(lws_http_transaction_completed(wsi))
                return 1;

            return 0;
        }
        default:
            return lws_callback_http_dummy(wsi, reason, user, in, len);
    }
}

bool Server::Private::init(lws_context* context)
{
    static const struct lws_protocol_vhost_options mime = {
        NULL,
        NULL,
        ".mjs",
        "application/javascript"
    };

    auto configHttpCallback =
        [] (lws* wsi, lws_callback_reasons reason, void* user, void* in, size_t len) -> int {
            lws_vhost* vhost = lws_get_vhost(wsi);
            Private* p = static_cast<Private*>(lws_get_vhost_user(vhost));

            return p->httpCallback(wsi, reason, user, in, len);
        };

    static const struct lws_protocols protocols[] = {
        { "default", lws_callback_http_dummy, 0, 0 },
        { "config_http", configHttpCallback, 0, 0 },
        nullptr// { nullptr, nullptr, 0, 0 }
    };

    static const lws_http_mount configMount = {
        nullptr,
        "/Config.js",
        nullptr,
        nullptr,
        "config_http",
        nullptr,
        &mime,
        nullptr,
        0,
        0,
        0,
        0,
        0,
        0,
        LWSMPRO_CALLBACK,
        sizeof("/Config.js") - 1,
        nullptr,
    };
    static const lws_http_mount mount = {
        &configMount,
        "/",
        config.wwwRoot.c_str(),
        "index.html",
        nullptr,
        nullptr,
        &mime,
        nullptr,
        0,
        0,
        0,
        0,
        0,
        0,
        LWSMPRO_FILE,
        sizeof("/") - 1,
        nullptr,
    };

    if(!context) {
        lws_context_creation_info info {};
        info.gid = -1;
        info.uid = -1;
        info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
#if defined(LWS_WITH_GLIB)
        info.options |= LWS_SERVER_OPTION_GLIB;
        info.foreign_loops = reinterpret_cast<void**>(&loop);
#endif

        contextPtr.reset(lws_create_context(&info));
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
        Log()->info("Starting HTTP server on port {}", config.port);

        lws_context_creation_info vhostInfo {};
        vhostInfo.port = config.port;
        vhostInfo.protocols = protocols;
        vhostInfo.mounts = &mount;
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
        Log()->info("Starting HTTPS server on port {}", config.securePort);

        lws_context_creation_info secureVhostInfo {};
        secureVhostInfo.port = config.securePort;
        secureVhostInfo.mounts = &mount;
        secureVhostInfo.error_document_404 = "/404.html";
        secureVhostInfo.ssl_cert_filepath = config.certificate.c_str();
        secureVhostInfo.ssl_private_key_filepath = config.key.c_str();
        secureVhostInfo.vhost_name = config.serverName.c_str();
        secureVhostInfo.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        secureVhostInfo.options |= LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
        secureVhostInfo.user = this;
        if(config.secureBindToLoopbackOnly)
            secureVhostInfo.iface = "lo";

        lws_vhost* secureVhost = lws_create_vhost(context, &secureVhostInfo);
        if(!secureVhost)
             return false;
    }

    return true;
}

Server::Server(
    const Config& config,
    unsigned short webRTSPPort,
    GMainLoop* loop) noexcept :
    _p(std::make_unique<Private>(this, config, webRTSPPort, loop))
{
}

Server::~Server()
{
}

bool Server::init(lws_context* context /*= nullptr*/) noexcept
{
    return _p->init(context);
}

}
