#pragma once

#include <QObject>
#include <QQuickItem>

#include "RtStreaming/GstRtStreaming/GstWebRTCPeer.h"


namespace webrtsp::qml {

class Peer : public GstWebRTCPeer
{
public:
    Peer(QQuickItem* view) noexcept : GstWebRTCPeer(Role::Viewer), _view(view) {}
    ~Peer() noexcept override {}

    using GstWebRTCPeer::prepare;
    using GstWebRTCPeer::play;

protected:
    void prepare(const WebRTCConfigPtr&) noexcept override;

private:
    QQuickItem *const _view;
};

}
