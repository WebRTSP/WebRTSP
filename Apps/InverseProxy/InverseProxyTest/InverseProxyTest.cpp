#include <thread>
#include <memory>

#include <CxxPtr/GlibPtr.h>

#include "Common/LwsLog.h"

#include "RtspParser/RtspParser.h"

#include "Signalling/Log.h"
#include "Signalling/WsServer.h"
#include "Signalling/ServerSession.h"

#include "Client/Log.h"
#include "Client/WsClient.h"
#include "Client/ClientSession.h"

#include "GstStreaming/LibGst.h"
#include "GstStreaming/GstRtspReStreamer.h"
#include "GstStreaming/GstClient.h"

#include "../InverseProxyServer/Log.h"
#include "../InverseProxyServer/InverseProxyServer.h"
#include "../InverseProxyAgent/Log.h"
#include "../InverseProxyAgent/InverseProxyAgent.h"

#define ENABLE_STREAMER 1
#define ENABLE_SERVER 1
#define ENABLE_VIEWER 1

enum {
    RECONNECT_TIMEOUT = 5,
};


#if ENABLE_VIEWER

struct TestClientSession: public ClientSession
{
    using ClientSession::ClientSession;

protected:
    bool onOptionsResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onListResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;

private:
    rtsp::Parameters _list;
};

bool TestClientSession::onOptionsResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(!ClientSession::onOptionsResponse(request, response))
        return false;

    if(!isSupported(rtsp::Method::LIST))
        return false;

    requestList();

    return true;
}

bool TestClientSession::onListResponse(
    const rtsp::Request&,
    const rtsp::Response& response) noexcept
{
    if(ResponseContentType(response) != "text/parameters")
        return false;

    if(!rtsp::ParseParameters(response.body, &_list))
        return false;

    if(_list.empty())
        return false;

    const guint32 randomStreamer = g_random_int_range(0, _list.size());

    setUri(std::next(_list.begin(), randomStreamer)->first);

    requestDescribe();

    return true;
}

static std::unique_ptr<WebRTCPeer> CreateClientPeer()
{
    return std::make_unique<GstClient>();
}

static std::unique_ptr<rtsp::ClientSession> CreateClientSession (
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return
        std::make_unique<TestClientSession>(
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
    enum {
        FRONT_SERVER_PORT = 8010,
        BACK_SERVER_PORT = 8011,
    };

    LibGst libGst;

    const spdlog::level::level_enum wsLogLevel = spdlog::level::warn;
    InitLwsLogger(wsLogLevel);
    const spdlog::level::level_enum logLevel = spdlog::level::trace;
    InitWsClientLogger(logLevel);
    InitWsServerLogger(logLevel);
    InitInverseProxyAgentLogger(logLevel);
    InitInverseProxyServerLogger(logLevel);

    const std::string server = "localhost";
    const std::string sourceName = "source1";
    const std::string streamerName = "bars";
    const std::string streamer2Name = "green";
    const std::string sourceAuthToken = "dummyToken";

#if ENABLE_SERVER
    std::thread serverThread(
        [&sourceName, &sourceAuthToken] () {
            InverseProxyServerConfig config {
                .frontPort = FRONT_SERVER_PORT,
                .backPort = BACK_SERVER_PORT,
//                .stunServer = "localhost:3478",
//                .turnServer = "localhost:3478",
//                .turnUsername = "anonymous",
//                .turnCredential = "guest",
//                .turnStaticAuthSecret = "dummySecret",
                .backAuthTokens = { {sourceName, sourceAuthToken} }
            };
            InverseProxyServerMain(config);
        });
#endif

#if ENABLE_STREAMER
    std::thread streamSourceClientThread(
        [&sourceName, &streamerName, &streamer2Name, &sourceAuthToken] () {
            InverseProxyAgentConfig config {};
            config.clientConfig.server = "localhost";
            config.clientConfig.serverPort = BACK_SERVER_PORT;
            config.name = sourceName;
            config.authToken = sourceAuthToken;
            config.streamers.emplace(
                streamerName,
                StreamerConfig { StreamerConfig::Type::Test, streamerName });
            config.streamers.emplace(
                streamer2Name,
                StreamerConfig { StreamerConfig::Type::Test, streamer2Name });

            InverseProxyAgentMain(config);
        });
#endif

#if ENABLE_VIEWER
    auto clientThreadMain =
        [&server, &sourceName] () {
            client::Config config {};
            config.server = server;
            config.serverPort = FRONT_SERVER_PORT;

            GMainContextPtr clientContextPtr(g_main_context_new());
            GMainContext* clientContext = clientContextPtr.get();
            g_main_context_push_thread_default(clientContext);
            GMainLoopPtr loopPtr(g_main_loop_new(clientContext, FALSE));
            GMainLoop* loop = loopPtr.get();

            client::WsClient client(
                config,
                loop,
                std::bind(
                    CreateClientSession,
                    std::placeholders::_1,
                    std::placeholders::_2),
                std::bind(ClientDisconnected, &client));

            if(client.init()) {
                client.connect();
                g_main_loop_run(loop);
            }
        };

    g_usleep(2 * G_USEC_PER_SEC);

    std::thread clientThread(clientThreadMain);

    g_usleep(2 * G_USEC_PER_SEC);

    std::thread client2Thread(clientThreadMain);
#endif

#if ENABLE_SERVER
    if(serverThread.joinable())
        serverThread.join();
#endif

#if ENABLE_STREAMER
    if(streamSourceClientThread.joinable())
        streamSourceClientThread.join();
#endif

#if ENABLE_VIEWER
    if(clientThread.joinable())
        clientThread.join();

    if(client2Thread.joinable())
        client2Thread.join();
#endif

    return 0;
}
