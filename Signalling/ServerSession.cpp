#include "ServerSession.h"

#include <list>
#include <map>

#include "RtspSession/StatusCode.h"
#include "RtspSession/IceCandidate.h"

#include "Log.h"


namespace {

struct MediaSession
{
    enum class Type {
        Describe,
        Record,
        Subscribe,
    };

    MediaSession(MediaSession::Type type, const std::string& uri) :
        type(type), uri(uri) {}

    const Type type;
    const std::string uri;
    std::unique_ptr<WebRTCPeer> localPeer;
    std::deque<rtsp::IceCandidate> iceCandidates;
    bool prepared = false;
};

typedef std::map<rtsp::MediaSessionId, std::unique_ptr<MediaSession>> MediaSessions;

const auto Log = ServerSessionLog;

}

struct ServerSession::Private
{
    struct AutoEraseRequest;
    struct AutoEraseRecordRequest;

    Private(
        ServerSession* owner,
        const CreatePeer& createPeer);
    Private(
        ServerSession* owner,
        const CreatePeer& createPeer,
        const CreatePeer& createRecordPeer);

    ServerSession *const owner;

    CreatePeer createPeer;
    CreatePeer createRecordPeer;

    std::optional<std::string> authCookie;

    MediaSessions mediaSessions;

    bool recordEnabled()
        { return createRecordPeer ? true : false; }

    std::string nextSessionId()
        { return std::to_string(_nextSessionId++); }

    void sendIceCandidates(const rtsp::MediaSessionId&, MediaSession* mediaSession);
    void streamerPrepared(rtsp::CSeq describeRequestCSeq, const rtsp::MediaSessionId&);
    void recorderPrepared(rtsp::CSeq recordRequestCSeq, const rtsp::MediaSessionId&);
    void recordToClientStreamerPrepared(const rtsp::MediaSessionId&);
    void iceCandidate(
        const rtsp::MediaSessionId&,
        unsigned, const std::string&);
    void eos(const rtsp::MediaSessionId& session);

private:
    unsigned _nextSessionId = 1;
};

ServerSession::Private::Private(
    ServerSession* owner,
    const CreatePeer& createPeer) :
    owner(owner),
    createPeer(createPeer)
{
}

ServerSession::Private::Private(
    ServerSession* owner,
    const CreatePeer& createPeer,
    const CreatePeer& createRecordPeer) :
    owner(owner),
    createPeer(createPeer),
    createRecordPeer(createRecordPeer)
{
}

void ServerSession::Private::sendIceCandidates(
    const rtsp::MediaSessionId& session,
    MediaSession* mediaSession)
{
    if(!mediaSession->iceCandidates.empty()) {
        std::string iceCandidates;
        for(const rtsp::IceCandidate& c : mediaSession->iceCandidates) {
            iceCandidates +=
                std::to_string(c.mlineIndex) + "/" + c.candidate + "\r\n";
        }

        if(!mediaSession->iceCandidates.empty()) {
            owner->requestSetup(
                mediaSession->uri,
                "application/x-ice-candidate",
                session,
                iceCandidates);
        }

        mediaSession->iceCandidates.clear();
    }
}

void ServerSession::Private::streamerPrepared(
    rtsp::CSeq describeRequestCSeq,
    const rtsp::MediaSessionId& session)
{
    auto it = mediaSessions.find(session);
    if(mediaSessions.end() == it || it->second->type != MediaSession::Type::Describe) {
        owner->disconnect();
        return;
    }

    MediaSession& mediaSession = *(it->second);
    WebRTCPeer& localPeer = *mediaSession.localPeer;

    mediaSession.prepared = true;

    if(localPeer.sdp().empty())
        owner->disconnect();
    else {
        rtsp::Response response;
        prepareOkResponse(describeRequestCSeq, session, &response);

        response.headerFields.emplace("Content-Type", "application/sdp");

        response.body = localPeer.sdp();

        owner->sendResponse(response);

        sendIceCandidates(session, &mediaSession);
    }
}

void ServerSession::Private::recorderPrepared(
    rtsp::CSeq recordRequestCSeq,
    const rtsp::MediaSessionId& session)
{
    auto it = mediaSessions.find(session);
    if(mediaSessions.end() == it || it->second->type != MediaSession::Type::Record) {
        owner->disconnect();
        return;
    }

    MediaSession& mediaSession = *(it->second);
    WebRTCPeer& recorder = *mediaSession.localPeer;

    mediaSession.prepared = true;

    if(recorder.sdp().empty())
        owner->disconnect();
    else {
        rtsp::Response response;
        prepareOkResponse(recordRequestCSeq, session, &response);

        response.headerFields.emplace("Content-Type", "application/sdp");

        response.body = recorder.sdp();

        owner->sendResponse(response);

        sendIceCandidates(session, &mediaSession);
    }
}

void ServerSession::Private::recordToClientStreamerPrepared(const rtsp::MediaSessionId& mediaSessionId)
{
    auto it = mediaSessions.find(mediaSessionId);
    assert(mediaSessions.end() != it);
    if(mediaSessions.end() == it) {
        return;
    }

    MediaSession& mediaSession = *(it->second);
    if(mediaSession.type != MediaSession::Type::Subscribe) {
        assert(false);
        return;
    }

    WebRTCPeer& localPeer = *mediaSession.localPeer;

    mediaSession.prepared = true;

    if(localPeer.sdp().empty()) {
        assert(false);
        owner->disconnect();
    } else {
        rtsp::Request& request =
            *owner->createRequest(rtsp::Method::RECORD, mediaSession.uri);

        request.headerFields.emplace("Session", mediaSessionId);
        request.headerFields.emplace("Content-Type", "application/sdp");

        request.body = localPeer.sdp();

        owner->sendRequest(request);

        sendIceCandidates(mediaSessionId, &mediaSession);
    }
}

void ServerSession::Private::iceCandidate(
    const rtsp::MediaSessionId& session,
    unsigned mlineIndex, const std::string& candidate)
{
    auto it = mediaSessions.find(session);
    if(mediaSessions.end() == it) {
        owner->disconnect();
        return;
    }

    MediaSession& mediaSession = *(it->second);
    if(mediaSession.prepared) {
        owner->requestSetup(
            mediaSession.uri,
            "application/x-ice-candidate",
            session,
            std::to_string(mlineIndex) + "/" + candidate + "\r\n");
    } else {
        mediaSession.iceCandidates.emplace_back(rtsp::IceCandidate { mlineIndex, candidate });
    }
}

void ServerSession::Private::eos(const rtsp::MediaSessionId& session)
{
    Log()->trace("[{}] Eos. Session: {}", owner->sessionLogId, session);

    auto it = mediaSessions.find(session);
    if(mediaSessions.end() == it) {
        owner->disconnect();
        return;
    }

    MediaSession& mediaSession = *(it->second);
    mediaSession.localPeer->stop();

    rtsp::Request& request =
        *owner->createRequest(rtsp::Method::TEARDOWN, mediaSession.uri, session);

    mediaSessions.erase(it);

    owner->sendRequest(request);
}


ServerSession::ServerSession(
    const WebRTCConfigPtr& webRTCConfig,
    const CreatePeer& createPeer,
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    rtsp::Session(webRTCConfig, sendRequest, sendResponse),
    _p(new Private(this, createPeer))
{
}

ServerSession::ServerSession(
    const WebRTCConfigPtr& webRTCConfig,
    const CreatePeer& createPeer,
    const CreatePeer& createRecordPeer,
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    rtsp::Session(webRTCConfig, sendRequest, sendResponse),
    _p(new Private(this, createPeer, createRecordPeer))
{
}

ServerSession::~ServerSession()
{
}

bool ServerSession::onConnected(const std::optional<std::string>& authCookie) noexcept
{
    _p->authCookie = authCookie;

    return rtsp::Session::onConnected();
}

const std::optional<std::string>& ServerSession::authCookie() const noexcept
{
    return _p->authCookie;
}

std::string ServerSession::nextSessionId()
{
    return _p->nextSessionId();
}

bool ServerSession::handleRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(requestPtr->method != rtsp::Method::RECORD && !authorize(requestPtr)) {
        Log()->error("[{}] {} authorize failed for \"{}\"", sessionLogId, rtsp::MethodName(requestPtr->method), requestPtr->uri);

        sendUnauthorizedResponse(requestPtr->cseq);

        return true;
    }

    if(isProxyRequest(*requestPtr)) {
        switch(requestPtr->method) {
        case rtsp::Method::DESCRIBE:
        case rtsp::Method::SETUP:
        case rtsp::Method::PLAY:
        case rtsp::Method::TEARDOWN:
            return handleProxyRequest(requestPtr);
        default:
            break;
        }
    }

    return Session::handleRequest(requestPtr);
}

bool ServerSession::onOptionsRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    rtsp::Response response;
    prepareOkResponse(requestPtr->cseq, rtsp::MediaSessionId(), &response);

    std::string options;
    if(listEnabled(requestPtr->uri))
        options = "LIST";

    const bool playEnabled = this->playEnabled(requestPtr->uri);
    if(playEnabled) {
        if(!options.empty())
            options += ", ";
        options += "DESCRIBE, PLAY";
    }

    const bool recordEnabled = this->recordEnabled(requestPtr->uri) && _p->recordEnabled();
    if(recordEnabled) {
        if(!options.empty())
            options += ", ";
        options += "RECORD";
    }

    const bool subscribeEnabled = this->subscribeEnabled(requestPtr->uri);
    if(subscribeEnabled) {
        if(!options.empty())
            options += ", ";
        options += "SUBSCRIBE";
    }

    if(playEnabled || recordEnabled || subscribeEnabled) {
        options += ", SETUP, TEARDOWN";
    }

    response.headerFields.emplace("Public", options);

    sendResponse(response);

    return true;
}

bool ServerSession::playEnabled(const std::string&) noexcept
{
    return true;
}

bool ServerSession::onDescribeRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::Request& request = *requestPtr.get();

    if(!playEnabled(request.uri)) {
        Log()->error("[{}] Playback is not supported for \"{}\"", sessionLogId, requestPtr->uri);
        return false;
    }

    std::unique_ptr<WebRTCPeer> peerPtr = _p->createPeer(requestPtr->uri);
    if(!peerPtr) {
        Log()->error("[{}] Failed to create peer for \"{}\"", sessionLogId, requestPtr->uri);
        return false;
    }

    const rtsp::MediaSessionId session = nextSessionId();

    auto emplacePair =
        _p->mediaSessions.emplace(
            session,
            std::make_unique<MediaSession>(MediaSession::Type::Describe, request.uri));
    if(!emplacePair.second)
        return false;

    MediaSession& mediaSession = *(emplacePair.first->second);
    mediaSession.localPeer = std::move(peerPtr);

    mediaSession.localPeer->prepare(
        webRTCConfig(),
        std::bind(
            &ServerSession::Private::streamerPrepared,
            _p.get(),
            request.cseq,
            session),
        std::bind(
            &ServerSession::Private::iceCandidate,
            _p.get(),
            session,
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(
            &ServerSession::Private::eos,
            _p.get(),
            session));

    return true;
}

bool ServerSession::recordEnabled(const std::string&) noexcept
{
    return false;
}

bool ServerSession::subscribeEnabled(const std::string&) noexcept
{
    return false;
}

bool ServerSession::authorize(const std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    return requestPtr->method != rtsp::Method::RECORD;
}

bool ServerSession::onRecordRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::Request& request = *requestPtr.get();

    if(!recordEnabled(requestPtr->uri) || !_p->recordEnabled())
        return false;

    if(!authorize(requestPtr)) {
        Log()->error("[{}] RECORD authorize failed for \"{}\"", sessionLogId, requestPtr->uri);
        return false;
    }

    const std::string& sdp = request.body;
    if(sdp.empty())
        return false;

    std::unique_ptr<WebRTCPeer> peerPtr = _p->createRecordPeer(requestPtr->uri);
    if(!peerPtr)
        return false;

    const std::string contentType = RequestContentType(*requestPtr);
    if(contentType != "application/sdp")
        return false;

    const rtsp::MediaSessionId session = nextSessionId();

    auto emplacePair =
        _p->mediaSessions.emplace(
            session,
            std::make_unique<MediaSession>(MediaSession::Type::Record, request.uri));
    if(!emplacePair.second)
        return false;

    MediaSession& mediaSession = *(emplacePair.first->second);
    mediaSession.localPeer = std::move(peerPtr);
    WebRTCPeer& localPeer = *(mediaSession.localPeer);

    localPeer.prepare(
        webRTCConfig(),
        std::bind(
            &ServerSession::Private::recorderPrepared,
            _p.get(),
            request.cseq,
            session),
        std::bind(
            &ServerSession::Private::iceCandidate,
            _p.get(),
            session,
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(
            &ServerSession::Private::eos,
            _p.get(),
            session));

    localPeer.setRemoteSdp(sdp);

    localPeer.play();

    return true;
}

bool ServerSession::onSetupRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::MediaSessionId session = RequestSession(*requestPtr);

    auto it = _p->mediaSessions.find(session);
    if(it == _p->mediaSessions.end())
        return false;

    WebRTCPeer& localPeer = *it->second->localPeer;

    if(RequestContentType(*requestPtr) != "application/x-ice-candidate")
        return false;

    const std::string& ice = requestPtr->body;

    std::string::size_type pos = 0;
    while(pos < ice.size()) {
        const std::string::size_type lineEndPos = ice.find("\r\n", pos);
        if(lineEndPos == std::string::npos)
            return false;

        const std::string line = ice.substr(pos, lineEndPos - pos);

        const std::string::size_type delimiterPos = line.find("/");
        if(delimiterPos == std::string::npos || 0 == delimiterPos)
            return false;

        try{
            const int idx = std::stoi(line.substr(0, delimiterPos));
            if(idx < 0)
                return false;

            const std::string candidate =
                line.substr(delimiterPos + 1);

            if(candidate.empty())
                return false;

            Log()->trace("[{}] Adding ice candidate \"{}\"", sessionLogId, candidate);

            localPeer.addIceCandidate(idx, candidate);
        } catch(...) {
            return false;
        }
        pos = lineEndPos + 2;
    }

    sendOkResponse(requestPtr->cseq, session);

    return true;
}

bool ServerSession::onPlayRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::MediaSessionId session = RequestSession(*requestPtr);
    if(session.empty())
        return false;

    auto it = _p->mediaSessions.find(session);
    if(it == _p->mediaSessions.end())
        return false;

    MediaSession& mediaSession = *it->second;
    if(mediaSession.type != MediaSession::Type::Describe)
        return false;

    if(RequestContentType(*requestPtr) != "application/sdp")
        return false;

    WebRTCPeer& localPeer = *(mediaSession.localPeer);

    localPeer.setRemoteSdp(requestPtr->body);
    localPeer.play();

    sendOkResponse(requestPtr->cseq, session);

    return true;
}

bool ServerSession::onTeardownRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::MediaSessionId session = RequestSession(*requestPtr);

    auto it = _p->mediaSessions.find(session);
    if(it == _p->mediaSessions.end())
        return false;

    WebRTCPeer& localPeer = *(it->second->localPeer);

    localPeer.stop();

    sendOkResponse(requestPtr->cseq, session);

    _p->mediaSessions.erase(it);

    return true;
}

void ServerSession::startRecordToClient(
    const std::string& uri,
    const rtsp::MediaSessionId& mediaSessionId) noexcept
{
    std::unique_ptr<WebRTCPeer> peerPtr = _p->createPeer(uri);
    if(!peerPtr) {
        onEos(); // FIXME! send TEARDOWN instead and remove Media Session
        return;
    }

    auto emplacePair =
        _p->mediaSessions.emplace(
            mediaSessionId,
            std::make_unique<MediaSession>(MediaSession::Type::Subscribe, uri));
    if(!emplacePair.second) {
        onEos(); // FIXME! send TEARDOWN instead and remove Media Session
        return;
    }

    MediaSession& mediaSession = *(emplacePair.first->second);
    mediaSession.localPeer = std::move(peerPtr);

    mediaSession.localPeer->prepare(
        webRTCConfig(),
        std::bind(
            &ServerSession::Private::recordToClientStreamerPrepared,
            _p.get(),
            mediaSessionId),
        std::bind(
            &ServerSession::Private::iceCandidate,
            _p.get(),
            mediaSessionId,
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(
            &ServerSession::Private::eos,
            _p.get(),
            mediaSessionId));
}

bool ServerSession::onRecordResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    const rtsp::MediaSessionId mediaSessionId = RequestSession(request);
    if(mediaSessionId.empty() || mediaSessionId != ResponseSession(response))
        return false;

    auto it = _p->mediaSessions.find(mediaSessionId);
    if(it == _p->mediaSessions.end())
        return false;

    MediaSession& mediaSession = *it->second;
    if(mediaSession.type != MediaSession::Type::Subscribe)
        return false;

    if(ResponseContentType(response) != "application/sdp")
        return false;

    WebRTCPeer& localPeer = *(mediaSession.localPeer);

    localPeer.setRemoteSdp(response.body);
    localPeer.play();

    return true;
}

void ServerSession::teardownMediaSession(const rtsp::MediaSessionId& mediaSession) noexcept
{
    assert(!mediaSession.empty());
    if(mediaSession.empty())
        return;

    const bool erased = _p->mediaSessions.erase(mediaSession) != 0;
    assert(erased);
}
