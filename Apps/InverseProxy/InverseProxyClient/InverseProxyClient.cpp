#include "InverseProxyClient.h"

#include <CxxPtr/GlibPtr.h>

#include "Client/WsClient.h"

#include "GstStreaming/GstTestStreamer.h"

#include "InverseProxyClientSession.h"


enum {
    DEFAULT_RECONNECT_TIMEOUT = 5,
};

static std::unique_ptr<WebRTCPeer> CreateInverseProxyClientPeer(const std::string& /*uri*/)
{
    return std::make_unique<GstTestStreamer>();
}

static std::unique_ptr<rtsp::ServerSession> CreateInverseProxyClientSession (
    const InverseProxyClientConfig* config,
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return
        std::make_unique<InverseProxyClientSession>(
            config->name,
            config->authToken,
            CreateInverseProxyClientPeer,
            sendRequest, sendResponse);
}

static void ClientDisconnected(
    const InverseProxyClientConfig* config,
    client::WsClient* client) noexcept
{
    const unsigned reconnectTimeout =
        config->reconnectTimeout > 0 ?
            config->reconnectTimeout :
            DEFAULT_RECONNECT_TIMEOUT;
    GSourcePtr timeoutSourcePtr(g_timeout_source_new_seconds(reconnectTimeout));
    GSource* timeoutSource = timeoutSourcePtr.get();
    g_source_set_callback(timeoutSource,
        [] (gpointer userData) -> gboolean {
            static_cast<client::WsClient*>(userData)->connect();
            return false;
        }, client, nullptr);
    g_source_attach(timeoutSource, g_main_context_get_thread_default());
}

int InverseProxyClientMain(const InverseProxyClientConfig& config)
{
    GMainContextPtr clientContextPtr(g_main_context_new());
    GMainContext* clientContext = clientContextPtr.get();
    g_main_context_push_thread_default(clientContext);
    GMainLoopPtr loopPtr(g_main_loop_new(clientContext, FALSE));
    GMainLoop* loop = loopPtr.get();

    client::WsClient client(
        config.clientConfig,
        loop,
        std::bind(
            CreateInverseProxyClientSession,
            &config,
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(ClientDisconnected, &config, &client));

    if(client.init()) {
        client.connect();
        g_main_loop_run(loop);
    } else
        return -1;

    return 0;
}
