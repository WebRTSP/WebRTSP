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
    static thread_local std::weak_ptr<QActor> sharedActor;

    if(sharedActor.expired()) {
        _actor = std::make_shared<QActor>();
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
    reset();
}

void Player::reset()
{
    if(!_peer)
        return;

    // use sendAction to be sure peer doestroed before possibler Player destroy
    _actor->sendAction([peer = std::move(_peer)] () mutable {
        peer.reset();
    });

    _describeCSeq = rtsp::InvalidCSeq;
    _mediaSession.clear();
}

void Player::play()
{
    if(_peer)
        return;

    _peer = std::make_shared<Peer>(_view, connection()->webRTCConfig());
    _peer->moveToThread(_actor->actorThread());

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

void Player::peerPrepared(const std::string& sdp)
{
    Q_ASSERT(connection()->isOpen());

    connection()->requestPlay(this, _encodedUri, _mediaSession, sdp);
}

void Player::iceCandidate(
    unsigned mlineIndex,
    const std::string& candidate)
{
    Q_ASSERT(connection()->isOpen());

    connection()->requestSetup(
        this,
        _encodedUri,
        _mediaSession,
        mlineIndex,
        candidate);
}

void Player::eos()
{
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

    QObject::connect(_peer.get(), &Peer::prepared, this, &Player::peerPrepared);
    QObject::connect(_peer.get(), &Peer::iceCandidate, this, &Player::iceCandidate);
    QObject::connect(_peer.get(), &Peer::eos, this, &Player::eos);
    QObject::connect(_peer.get(), &Peer::canPlay, this, &Player::canPlay);

    _actor->postAction([peer = _peer, sdp] () mutable {
        peer->prepare();
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
