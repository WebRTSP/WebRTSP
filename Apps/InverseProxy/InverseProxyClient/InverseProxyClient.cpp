#include "InverseProxyClient.h"

#include <CxxPtr/GlibPtr.h>

#include "Client/WsClient.h"

#include "GstStreaming/GstTestStreamer.h"

#include "InverseProxyClientSession.h"


enum {
    RECONNECT_TIMEOUT = 5,
};


static std::unique_ptr<WebRTCPeer> CreateInverseProxyClientPeer(const std::string& /*uri*/)
{
    return std::make_unique<GstTestStreamer>();
}

static std::unique_ptr<rtsp::ServerSession> CreateInverseProxyClientSession (
    const std::string& clientName,
    const std::string& authToken,
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return
        std::make_unique<InverseProxyClientSession>(
            clientName,
            authToken,
            CreateInverseProxyClientPeer,
            sendRequest, sendResponse);
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

int InverseProxyClientMain(
    const client::Config& config,
    const std::string& clientName,
    const std::string& authToken)
{
    GMainContextPtr clientContextPtr(g_main_context_new());
    GMainContext* clientContext = clientContextPtr.get();
    g_main_context_push_thread_default(clientContext);
    GMainLoopPtr loopPtr(g_main_loop_new(clientContext, FALSE));
    GMainLoop* loop = loopPtr.get();

    client::WsClient client(
        config,
        loop,
        std::bind(
            CreateInverseProxyClientSession,
            clientName,
            authToken,
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(ClientDisconnected, &client));

    if(client.init()) {
        client.connect();
        g_main_loop_run(loop);
    } else
        return -1;

    return 0;
}
