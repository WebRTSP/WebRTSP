#include "Proxy.h"

#include <CxxPtr/GlibPtr.h>

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

int ProxyMain(const ProxyConfig& config)
{
    GMainContextPtr serverContextPtr(g_main_context_new());
    GMainContext* serverContext = serverContextPtr.get();
    g_main_context_push_thread_default(serverContext);
    GMainLoopPtr loopPtr(g_main_loop_new(serverContext, FALSE));
    GMainLoop* loop = loopPtr.get();

    signalling::WsServer server(config, loop, CreateProxySession);

    if(server.init())
        g_main_loop_run(loop);
    else
        return -1;

    return 0;
}
