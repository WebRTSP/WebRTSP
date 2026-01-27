#pragma once

#include <QObject>
#include <QtQml>

#include "Helpers/Actor.h"

#include "Peer.h"
#include "ConnectionClient.h"


namespace webrtsp::qml {

class Connection;
class Player: public ConnectionClient
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WebRTSPPlayer)
    QML_UNCREATABLE("Type is only available as property value")

public:
    Player(Connection*, const QString& uri, QQuickItem* view) noexcept;
    ~Player() noexcept;

private:
    class WebRTSPPeer;

    void onConnected() noexcept override;
    void onDisconnected() noexcept override;

    bool onDescribeResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;
    bool onPlayResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;

    bool onSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

private slots:
    void onReceiverPrepared(const std::shared_ptr<Peer>&, const std::string& sdp);
    void onIceCandidate(const std::shared_ptr<Peer>&, unsigned, const std::string& candidate);
    void onEos(const std::shared_ptr<Peer>&);

private:
    const QString _uri;
    const std::string _encodedUri;
    QPointer<QQuickItem> _view;

    std::shared_ptr<Actor> _actor;
    std::shared_ptr<Peer> _peer;

    rtsp::CSeq _describeCSeq = rtsp::InvalidCSeq;

    rtsp::MediaSessionId _mediaSession;
};

}
