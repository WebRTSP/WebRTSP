#include <thread>
#include <memory>

#include <CxxPtr/GlibPtr.h>

#include "RtStreaming/GstRtStreaming/LibGst.h"

#include "TestParse.h"
#include "TestSerialize.h"

#define ENABLE_SERVER 1
#define ENABLE_CLIENT 1
#define USE_RTSP_RESTREAMER 0
#define USE_RESTREAMER 1

#if ENABLE_SERVER
    #include "Signalling/Log.h"
    #include "Signalling/WsServer.h"
    #include "Signalling/ServerSession.h"

    #include "RtStreaming/GstRtStreaming/GstTestStreamer.h"
    #include "RtStreaming/GstRtStreaming/GstReStreamer.h"
#endif

#if ENABLE_CLIENT
    #include "RtStreaming/GstRtStreaming/GstClient.h"

    #include "Client/Log.h"
    #include "Client/WsClient.h"
    #include "Client/ClientSession.h"
#endif

#if ENABLE_SERVER
    #define SERVER_HOST "localhost"
#else
    #define SERVER_HOST "ipcam.stream"
#endif

enum {
    SERVER_PORT = 5554,
    RECONNECT_TIMEOUT = 5,
};


#if ENABLE_SERVER
static std::unique_ptr<WebRTCPeer> CreateServerPeer(const std::string& uri)
{
#if USE_RTSP_RESTREAMER
    const std::string rtspSource =
        uri;
    return std::make_unique<GstRtspReStreamer>(rtspSource);
#elif USE_RESTREAMER
    return std::make_unique<GstReStreamer>(uri);
#else
    return std::make_unique<GstTestStreamer>();
#endif
}

static std::unique_ptr<rtsp::ServerSession> CreateServerSession (
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return std::make_unique<ServerSession>(CreateServerPeer, sendRequest, sendResponse);
}
#endif

#if ENABLE_CLIENT
static std::unique_ptr<WebRTCPeer> CreateClientPeer()
{
    return std::make_unique<GstClient>();
}

static std::unique_ptr<rtsp::ClientSession> CreateClientSession (
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    const std::string url =
#if USE_RTSP_RESTREAMER || USE_RESTREAMER
        "rtsp://ipcam.stream:8554/bars";
#elif ENABLE_SERVER
        "*";
#else
        "Bars";
#endif
    return
        std::make_unique<ClientSession>(
            url,
            CreateClientPeer,
            sendRequest,
            sendResponse);
}

static void ClientDisconnected(client::WsClient* client) noexcept
{
    GSourcePtr timeoutSourcePtr(g_timeout_source_new_seconds(RECONNECT_TIMEOUT));
    GSource* timeoutSource = timeoutSourcePtr.get();
    g_source_set_callback(timeoutSource,
        [] (gpointer userData) -> gboolean {
            static_cast<client::WsClient*>(userData)->connect();
            return false;
        }, client, nullptr);
    g_source_attach(timeoutSource, g_main_context_get_thread_default());
}
#endif

int main(int argc, char *argv[])
{
    LibGst libGst;

    TestParse();
    TestSerialize();

#if ENABLE_SERVER
    std::thread signallingThread(
        [] () {
            InitWsServerLogger(spdlog::level::trace);

            signalling::Config config {};
            config.port = SERVER_PORT;

            GMainContextPtr serverContextPtr(g_main_context_new());
            GMainContext* serverContext = serverContextPtr.get();
            g_main_context_push_thread_default(serverContext);
            GMainLoopPtr loopPtr(g_main_loop_new(serverContext, FALSE));
            GMainLoop* loop = loopPtr.get();

            signalling::WsServer server(config, loop, CreateServerSession);

            if(server.init())
                g_main_loop_run(loop);
        });
#endif

#if ENABLE_CLIENT
    std::thread clientThread(
        [] () {
            InitWsClientLogger(spdlog::level::trace);

            client::Config config {};
            config.server = SERVER_HOST;
            config.serverPort = SERVER_PORT;

            GMainContextPtr clientContextPtr(g_main_context_new());
            GMainContext* clientContext = clientContextPtr.get();
            g_main_context_push_thread_default(clientContext);
            GMainLoopPtr loopPtr(g_main_loop_new(clientContext, FALSE));
            GMainLoop* loop = loopPtr.get();

            client::WsClient client(
                config,
                loop,
                CreateClientSession,
                std::bind(ClientDisconnected, &client));

            if(client.init()) {
                client.connect();
                g_main_loop_run(loop);
            }
        });
#endif

#if ENABLE_SERVER
    if(signallingThread.joinable())
        signallingThread.join();
#endif

#if ENABLE_CLIENT
    if(clientThread.joinable())
        clientThread.join();
#endif

    return 0;
}
