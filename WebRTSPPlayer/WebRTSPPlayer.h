#pragma once

#include <memory>

#include <glib.h>

#include "RtStreaming/WebRTCPeer.h"


class WebRTSPPlayer final
{
public:
    struct PeerFactory {
        virtual std::unique_ptr<WebRTCPeer> createPeer() noexcept = 0;
    };

    struct GstClientPeerFactory;

    // добавить передачу webrtcconfig
    WebRTSPPlayer(GMainLoop*, const WebRTCConfigPtr&, PeerFactory*) noexcept;
    ~WebRTSPPlayer() noexcept;

    bool connect(const std::string& url);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
