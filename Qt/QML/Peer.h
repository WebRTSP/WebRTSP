#pragma once

#include <QObject>
#include <QQuickItem>

#include "RtStreaming/GstRtStreaming/GstWebRTCPeer.h"


namespace webrtsp::qml {

class Peer : public QObject, public GstWebRTCPeer
{
    Q_OBJECT

public:
    Peer(QQuickItem* view, const WebRTCConfigPtr& webRTCConfig) noexcept :
        GstWebRTCPeer(Role::Viewer), _view(view), _webRTCConfig(webRTCConfig) {}
    ~Peer() noexcept override {}

    void prepare() noexcept;

    using GstWebRTCPeer::play;

signals:
    void prepared(const std::string& sdp);
    void iceCandidate(unsigned mlineIndex, const std::string& candidate);
    void eos();
    void canPlay();

protected:
    void prepare(const WebRTCConfigPtr&) noexcept override;

    gboolean onBusMessage(GstMessage*) noexcept override;

private:
    static void postCanPlay(GstElement*);

private:
    QQuickItem *const _view;
    const WebRTCConfigPtr _webRTCConfig;
};

}
