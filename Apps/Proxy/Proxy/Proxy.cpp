#include "Proxy.h"

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/libwebsocketsPtr.h>

#include "Http/HttpServer.h"

#include "Signalling/WsServer.h"
#include "Signalling/ServerSession.h"

#include "GstStreaming/GstReStreamer.h"

#include "Log.h"

static const auto Log = ProxyLog;

static std::unique_ptr<WebRTCPeer> CreateProxyPeer(const std::string& uri)
{
    return std::make_unique<GstReStreamer>(uri);
}

static std::unique_ptr<rtsp::ServerSession> CreateProxySession (
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return std::make_unique<ServerSession>(CreateProxyPeer, sendRequest, sendResponse);
}

int ProxyMain(const http::Config& httpConfig, const ProxyConfig& config)
{
    GMainContextPtr contextPtr(g_main_context_new());
    GMainContext* context = contextPtr.get();
    g_main_context_push_thread_default(context);

    GMainLoopPtr loopPtr(g_main_loop_new(context, FALSE));
    GMainLoop* loop = loopPtr.get();

    lws_context_creation_info lwsInfo {};
    lwsInfo.gid = -1;
    lwsInfo.uid = -1;
    lwsInfo.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
#if defined(LWS_WITH_GLIB)
    lwsInfo.options |= LWS_SERVER_OPTION_GLIB;
    lwsInfo.foreign_loops = reinterpret_cast<void**>(&loop);
#endif

    LwsContextPtr lwsContextPtr(lws_create_context(&lwsInfo));
    lws_context* lwsContext = lwsContextPtr.get();

    http::Server httpServer(httpConfig, loop);
    signalling::WsServer server(config, loop, CreateProxySession);

    if(httpServer.init(lwsContext) && server.init(lwsContext))
        g_main_loop_run(loop);
    else
        return -1;

    return 0;
}
