#include <thread>

#include <CxxPtr/GlibPtr.h>

#include "Client/Client.h"
#include "Signalling/WsServer.h"
#include "Signalling/ServerSession.h"

#include "TestParse.h"
#include "TestSerialize.h"

#define ENABLE_CLIENT 1

static std::unique_ptr<rtsp::ServerSession> CreateSession (
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept
{
    return std::make_unique<ServerSession>(sendRequest, sendResponse);
}

int main(int argc, char *argv[])
{
    enum {
        SERVER_PORT = 8081,
    };

    TestParse();
    TestSerialize();

    std::thread signallingThread(
        [] () {
            signalling::Config config {};
            config.port = SERVER_PORT;

            GMainLoopPtr loopPtr(g_main_loop_new(nullptr, FALSE));
            GMainLoop* loop = loopPtr.get();

            signalling::WsServer server(config, loop, CreateSession);

            if(server.init())
                g_main_loop_run(loop);
        });

#if ENABLE_CLIENT
    std::thread clientThread(
        [] () {
            client::Config config {};
            config.server = "localhost";
            config.serverPort = SERVER_PORT;

            client::Client(&config);
        });
#endif

    if(signallingThread.joinable())
        signallingThread.join();

#if ENABLE_CLIENT
    if(clientThread.joinable())
        clientThread.join();
#endif

    return 0;
}
