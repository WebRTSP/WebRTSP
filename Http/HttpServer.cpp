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
    Private(Server*, const Config&, GMainLoop*);

    bool init(lws_context* context);

    Server *const owner;
    Config config;
    GMainLoop* loop;

#if !defined(LWS_WITH_GLIB)
    LwsSourcePtr lwsSourcePtr;
#endif
    LwsContextPtr contextPtr;
};

Server::Private::Private(
    Server* owner,
    const Config& config,
    GMainLoop* loop) :
    owner(owner), config(config), loop(loop)
{
}

bool Server::Private::init(lws_context* context)
{
    static const struct lws_protocol_vhost_options mime = {
        NULL,
        NULL,
        ".mjs",
        "application/javascript"
    };

    static const lws_http_mount mount = {
        nullptr,
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
        1,
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
        vhostInfo.mounts = &mount;
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
    GMainLoop* loop) noexcept :
    _p(std::make_unique<Private>(this, config, loop))
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
