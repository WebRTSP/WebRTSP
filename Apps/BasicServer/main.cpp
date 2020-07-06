#include <memory>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/libwebsocketsPtr.h>

#include "Common/LwsLog.h"
#include "Http/Log.h"
#include "Http/HttpServer.h"
#include "Signalling/Log.h"
#include "Signalling/WsServer.h"
#include "Signalling/ServerSession.h"
#include "GstStreaming/LibGst.h"
#include "GstStreaming/GstTestStreamer.h"


static std::unique_ptr<WebRTCPeer> CreatePeer(const std::string& uri)
{
    return std::make_unique<GstTestStreamer>(uri);
}

static std::unique_ptr<rtsp::ServerSession> CreateSession (
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept
{
    return std::make_unique<ServerSession>(CreatePeer, sendRequest, sendResponse);
}

int main(int argc, char *argv[])
{
    LibGst libGst;

    http::Config httpConfig {};
    signalling::Config config {};

    GMainLoopPtr loopPtr(g_main_loop_new(nullptr, FALSE));
    GMainLoop* loop = loopPtr.get();

    lws_context_creation_info lwsInfo {};
    lwsInfo.gid = -1;
    lwsInfo.uid = -1;
    lwsInfo.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
#if defined(LWS_WITH_GLIB)
    lwsInfo.options |= LWS_SERVER_OPTION_GLIB;
    lwsInfo.foreign_loops = reinterpret_cast<void**>(&loop);
#endif

    LwsContextPtr contextPtr(lws_create_context(&lwsInfo));
    lws_context* context = contextPtr.get();

    http::Server httpServer(httpConfig, loop);
    signalling::WsServer server(config, loop, CreateSession);

    if(httpServer.init(context) && server.init(context))
        g_main_loop_run(loop);
    else
        return -1;

    return 0;
}
