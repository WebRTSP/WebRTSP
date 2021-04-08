#include <thread>
#include <memory>

#include <CxxPtr/GlibPtr.h>

#include "RtcStreaming/GstRtcStreaming/LibGst.h"

#define ENABLE_SERVER 1
#define ENABLE_CLIENT 1
#define USE_RESTREAMER 0

#if ENABLE_SERVER
    #include "Signalling/Log.h"
    #include "Signalling/WsServer.h"
    #include "Signalling/ServerSession.h"

    #include "RtcStreaming/GstRtcStreaming/GstTestStreamer.h"
    #include "RtcStreaming/GstRtcStreaming/GstReStreamer.h"
#endif

#if ENABLE_CLIENT
    #include "RtcStreaming/GstRtcStreaming/GstClient.h"

    #include "Client/Log.h"
    #include "Client/WsClient.h"
    #include "Client/ClientRecordSession.h"
#endif

#if ENABLE_SERVER
    #define SERVER_HOST "localhost"
#else
    #define SERVER_HOST "ipcam.stream"
#endif

enum {
    SERVER_PORT = 5554,
    RECONNECT_TIMEOUT = 10,
};


#if ENABLE_SERVER
static std::unique_ptr<WebRTCPeer> CreateServerPeer(const std::string&)
{
    return std::make_unique<GstTestStreamer>();
}

static std::unique_ptr<WebRTCPeer> CreateServerRecordPeer(const std::string&)
{
    return std::make_unique<GstClient>();
}


static std::unique_ptr<rtsp::ServerSession> CreateServerSession (
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return
        std::make_unique<ServerSession>(
            CreateServerPeer,
            CreateServerRecordPeer,
            sendRequest,
            sendResponse);
}
#endif

#if ENABLE_CLIENT

struct TestRecordSession : public ClientRecordSession
{
    using ClientRecordSession::ClientRecordSession;

    bool onOptionsResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;
};

bool TestRecordSession::onOptionsResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(!ClientRecordSession::onOptionsResponse(request, response))
        return false;

    startRecord();

    return true;
}

static std::unique_ptr<WebRTCPeer> CreateClientPeer(const std::string& uri)
{
#if USE_RESTREAMER
    return std::make_unique<GstReStreamer>(uri);
#else
    return std::make_unique<GstTestStreamer>();
#endif
}

static std::unique_ptr<rtsp::ClientSession> CreateClientSession (
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    const std::string uri =
#if USE_RESTREAMER
        "rtsp://ipcam.stream:8554/bars";
#else
        "Bars";
#endif
    return
        std::make_unique<TestRecordSession>(
            uri,
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

#if ENABLE_SERVER
    std::thread signallingThread(
        [] () {
            InitWsServerLogger(spdlog::level::info);

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
            InitWsClientLogger(spdlog::level::info);

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
