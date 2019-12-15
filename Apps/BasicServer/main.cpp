#include <memory>

#include <CxxPtr/GlibPtr.h>

#include "Signalling/WsServer.h"
#include "Signalling/ServerSession.h"
#include "GstStreaming/GstTestStreamer.h"


static std::unique_ptr<WebRTCPeer> CreatePeer()
{
    return std::make_unique<GstTestStreamer>();
}

static std::unique_ptr<rtsp::ServerSession> CreateSession (
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept
{
    return std::make_unique<ServerSession>(CreatePeer, sendRequest, sendResponse);
}

int main(int argc, char *argv[])
{
    enum {
        SERVER_PORT = 8081,
    };

    signalling::Config config {};
    config.port = SERVER_PORT;

    GMainLoopPtr loopPtr(g_main_loop_new(nullptr, FALSE));
    GMainLoop* loop = loopPtr.get();

    signalling::WsServer server(config, loop, CreateSession);

    if(server.init())
        g_main_loop_run(loop);
    else
        return -1;

    return 0;
}
