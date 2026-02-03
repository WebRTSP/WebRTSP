#include "Player.h"

#include "RtspParser/RtspParser.h"

#include "Connection.h"


using namespace webrtsp::qml;

Player::Player(
    Connection* connection,
    const QString& uri,
    QQuickItem* view) noexcept :
    ConnectionClient(connection),
    _uri(uri),
    _encodedUri(uri == "*" ? uri.toStdString() : QUrl::toPercentEncoding(uri).toStdString()),
    _view(view)
{
    static thread_local std::weak_ptr<Actor> sharedActor;

    if(sharedActor.expired()) {
        _actor = std::make_shared<Actor>();
        sharedActor = _actor;
    } else {
        _actor = sharedActor.lock();
    }

    if(connection->isOpen()) {
        onConnected();
    }
}

Player::~Player() noexcept
{
}

void Player::reset()
{
    if(!_peer)
        return;

    _actor->postAction([peer = _peer] () mutable {
        peer.reset();
    });

    _peer.reset();
    _describeCSeq = rtsp::InvalidCSeq;
    _mediaSession.clear();
}

void Player::play()
{
    if(_peer)
        return;

    _peer = std::make_shared<Peer>(_view);

    _describeCSeq = connection()->requestDescribe(this, _encodedUri);
}

void Player::onConnected() noexcept
{
    play();
}

void Player::onDisconnected() noexcept
{
    reset();
}

void Player::onReceiverPrepared(
    const std::shared_ptr<Peer>& peer,
    const std::string& sdp)
{
    if(_peer != peer)
        return;

    Q_ASSERT(connection()->isOpen());

    connection()->requestPlay(this, _encodedUri, _mediaSession, sdp);
}

void Player::onIceCandidate(
    const std::shared_ptr<Peer>& peer,
    unsigned mlineIndex,
    const std::string& candidate)
{
    if(_peer != peer)
        return;

    Q_ASSERT(connection()->isOpen());

    connection()->requestSetup(
        this,
        _encodedUri,
        _mediaSession,
        mlineIndex,
        candidate);
}

void Player::onEos(const std::shared_ptr<Peer>& peer)
{
    if(_peer != peer)
        return;

    Q_ASSERT(connection()->isOpen());

    qInfo() << "EOS"; // FIXME! add handler
}

bool Player::onDescribeResponse(
    const rtsp::Request&,
    const rtsp::Response& response) noexcept
{
    if(response.cseq != _describeCSeq)
        return false;

    if(response.statusCode != rtsp::StatusCode::OK) {
        reset();
        qInfo().nospace()
            << "DESCRIBE request failed for " << _uri << ": "
            << response.reasonPhrase; // FIXME! add handler
        return true;
    }

    Q_ASSERT(_mediaSession.empty());

    _mediaSession = ResponseSession(response);
    if(_mediaSession.empty())
        return false;

    const std::string& sdp = response.body;
    if(sdp.empty())
        return false;

    _actor->postAction([
        owner = this,
        peer = _peer,
        webRTCConfig = connection()->webRTCConfig(),
        sdp
    ] () {
        peer->prepare(
            webRTCConfig,
            [owner, peer] () { // receiverPrepared
                QMetaObject::invokeMethod(
                    owner,
                    &Player::onReceiverPrepared,
                    peer,
                    peer->sdp());
            },
            [owner, peer] (unsigned mlineIndex, const std::string& candidate) { // iceCandidate
                QMetaObject::invokeMethod(
                    owner,
                    &Player::onIceCandidate,
                    peer,
                    mlineIndex,
                    candidate);
            },
            [owner, peer] () { // eos
                QMetaObject::invokeMethod(
                    owner,
                    &Player::onEos,
                    peer);
            },
            std::string());

        peer->setRemoteSdp(sdp);
    });

    return true;
}

bool Player::onPlayResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    if(rtsp::ResponseSession(response) != _mediaSession)
        return false;

    _actor->postAction([peer = _peer] () {
        peer->play();
    });

    return true;
}

bool Player::onSetupRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(RequestContentType(*requestPtr) != rtsp::IceCandidateContentType)
        return false;

    std::optional<std::pair<unsigned, std::string>> iceCandidate =
        rtsp::ParseIceCandidate(requestPtr->body);
    if(!iceCandidate)
        return false;

    _actor->postAction([peer = _peer, iceCandidate = iceCandidate.value()] () {
        peer->addIceCandidate(iceCandidate.first, iceCandidate.second);
    });

    connection()->sendOkResponse(requestPtr->cseq, rtsp::RequestSession(*requestPtr));

    return true;
}

bool Player::onTeardownRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    connection()->sendOkResponse(requestPtr->cseq, rtsp::RequestSession(*requestPtr));

    if(rtsp::RequestSession(*requestPtr) == _mediaSession) {
        reset();
        qInfo() << "TEARDOWN"; // FIXME! add handler
    }

    return true;
}
