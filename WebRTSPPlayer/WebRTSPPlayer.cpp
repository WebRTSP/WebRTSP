#include "WebRTSPPlayer.h"

#include "CxxPtr/GlibPtr.h"
#include "Signalling/WsClient.h"
#include "RtspSession/ClientSession.h"

#include "Log.h"


namespace
{
    auto Log = WebRTSPPlayerLog;

    enum {
        MIN_RECONNECT_TIMEOUT = 3, // seconds
        MAX_RECONNECT_TIMEOUT = 10, // seconds
    };

    struct Config: public WsClientConfig, public rtsp::Credentials
    {
        std::string uri;
    };

    class Session: public rtsp::ClientSession
    {
    public:
        Session(
            const Config&,
            const WebRTCConfigPtr&,
            WebRTSPPlayer::PeerFactory*,
            const SendRequest& ,
            const SendResponse&) noexcept;
    };

    Session::Session(
        const Config& config,
        const WebRTCConfigPtr& webRTCConfig,
        WebRTSPPlayer::PeerFactory* peerFactory,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept:
        rtsp::ClientSession(
            config.uri,
            webRTCConfig,
            config,
            [peerFactory] { return peerFactory->createPeer(); },
            sendRequest,
            sendResponse)
    {
    }
}

struct WebRTSPPlayer::Private: public WsClient::SessionFactory
{
    Private(
        GMainLoop* loop,
        const WebRTCConfigPtr& webRTCConfig,
        PeerFactory* peerFactory) noexcept:
        loop(loop), webRTCConfig(webRTCConfig), peerFactory(peerFactory) {}
    ~Private() noexcept;

    std::unique_ptr<rtsp::Session> createSession(
        const rtsp::Session::SendRequest& sendRequest,
        const rtsp::Session::SendResponse& sendResponse) noexcept override
    {
        return std::make_unique<Session>(
            config,
            webRTCConfig,
            peerFactory,
            sendRequest,
            sendResponse);
    }

    void onDisconnect(WsClient& client, unsigned /*statusCode*/) noexcept;

    GMainLoop *const loop;
    const WebRTCConfigPtr webRTCConfig;
    PeerFactory *const peerFactory;
    Config config;
    std::unique_ptr<WsClient> client;
    GSourcePtr reconnectTimeoutSourcePtr;
};

WebRTSPPlayer::Private::~Private() noexcept
{
    if(reconnectTimeoutSourcePtr) {
        g_source_destroy(reconnectTimeoutSourcePtr.get());
        reconnectTimeoutSourcePtr.reset();
    }

    // to prevent unexpected reconnect
    std::unique_ptr<WsClient> client = std::move(this->client);
    client.reset();
}

void WebRTSPPlayer::Private::onDisconnect(WsClient& client, unsigned /*statusCode*/) noexcept
{
    if(reconnectTimeoutSourcePtr) {
        Log()->warn("Trying to create new reconnect timout source while previous one is still active");
        return;
    }

    assert(this->client);

    const unsigned reconnectTimeout =
        g_random_int_range(MIN_RECONNECT_TIMEOUT, MAX_RECONNECT_TIMEOUT + 1);
    Log()->info("Scheduling reconnect within {} seconds...", reconnectTimeout);
    reconnectTimeoutSourcePtr.reset(g_timeout_source_new_seconds(reconnectTimeout));
    GSource* timeoutSource = reconnectTimeoutSourcePtr.get();
    g_source_set_callback(timeoutSource,
        [] (gpointer userData) -> gboolean {
            Private* p = static_cast<Private*>(userData);
            p->reconnectTimeoutSourcePtr.reset();
            if(p->client)
                p->client->connect();
            return G_SOURCE_REMOVE;
        }, this, nullptr);
    g_source_attach(timeoutSource, g_main_context_get_thread_default());
}

WebRTSPPlayer::WebRTSPPlayer(
    GMainLoop* loop,
    const WebRTCConfigPtr& webRTCConfig,
    PeerFactory* peerFactory) noexcept :
    _p(std::make_unique<Private>(loop, webRTCConfig, peerFactory))
{
}

WebRTSPPlayer::~WebRTSPPlayer() noexcept
{
}

bool WebRTSPPlayer::connect(const std::string& url)
{
    if(_p->client)
        return false; // FIXME?

    Config config;
    if(!WebRTSPUrlParse(url, &config, &config, &config.uri))
        return false; // FIXME?

    _p->config = std::move(config);

    std::unique_ptr<WsClient> client = std::make_unique<WsClient>(
        _p->config,
        _p.get(),
        [this] (WsClient& client, unsigned statusCode) {
            if(!_p->client)
                return;

            assert(_p->client.get() == &client);
            _p->onDisconnect(client, statusCode);
        });

    if(!client->init(_p->loop))
        return false; // FIXME?

    _p->client = std::move(client);
    _p->client->connect();

    return true;
}
