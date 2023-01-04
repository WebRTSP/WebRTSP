#include "ServerSession.h"

#include <list>
#include <map>

#include "RtspSession/StatusCode.h"
#include "RtspSession/IceCandidate.h"

#include "Log.h"


namespace {

struct MediaSession
{
    bool recorder = false;
    std::string uri;
    std::unique_ptr<rtsp::Request> createRequest; // describe or announce
    std::unique_ptr<WebRTCPeer> localPeer;
    std::deque<rtsp::IceCandidate> iceCandidates;
    bool prepared = false;
};

typedef std::map<rtsp::SessionId, std::unique_ptr<MediaSession>> MediaSessions;

struct RequestInfo
{
    std::unique_ptr<rtsp::Request> requestPtr;
    const rtsp::SessionId session;
};

typedef std::map<rtsp::CSeq, RequestInfo> Requests;

const auto Log = ServerSessionLog;

}

struct ServerSession::Private
{
    struct AutoEraseRequest;
    struct AutoEraseRecordRequest;

    Private(
        ServerSession* owner,
        const IceServers& iceServers,
        const CreatePeer& createPeer);
    Private(
        ServerSession* owner,
        const IceServers& iceServers,
        const CreatePeer& createPeer,
        const CreatePeer& createRecordPeer);

    ServerSession *const owner;
    const std::deque<std::string> iceServers;
    CreatePeer createPeer;
    CreatePeer createRecordPeer;

    std::optional<std::string> authCookie;

    Requests describeRequests;
    Requests recordRequests;
    MediaSessions mediaSessions;

    bool recordEnabled()
        { return createRecordPeer ? true : false; }

    std::string nextSession()
        { return std::to_string(_nextSession++); }

    void sendIceCandidates(const rtsp::SessionId&, MediaSession* mediaSession);
    void streamerPrepared(rtsp::CSeq describeRequestCSeq);
    void recorderPrepared(rtsp::CSeq recordRequestCSeq);
    void iceCandidate(
        const rtsp::SessionId&,
        unsigned, const std::string&);
    void eos(const rtsp::SessionId& session);

private:
    unsigned _nextSession = 1;
};

struct ServerSession::Private::AutoEraseRequest
{
    AutoEraseRequest(
        ServerSession::Private* owner,
        Requests::const_iterator it) :
        _owner(owner), _it(it) {}
    ~AutoEraseRequest()
        { if(_owner) _owner->describeRequests.erase(_it); }
    void discard()
        { _owner = nullptr; }

private:
    ServerSession::Private* _owner;
    Requests::const_iterator _it;
};

struct ServerSession::Private::AutoEraseRecordRequest
{
    AutoEraseRecordRequest(
        ServerSession::Private* owner,
        Requests::const_iterator it) :
        _owner(owner), _it(it) {}
    ~AutoEraseRecordRequest()
        { if(_owner) _owner->recordRequests.erase(_it); }
    void discard()
        { _owner = nullptr; }

private:
    ServerSession::Private* _owner;
    Requests::const_iterator _it;
};

ServerSession::Private::Private(
    ServerSession* owner,
    const IceServers& iceServers,
    const CreatePeer& createPeer) :
    owner(owner),
    iceServers(iceServers),
    createPeer(createPeer)
{
}

ServerSession::Private::Private(
    ServerSession* owner,
    const IceServers& iceServers,
    const CreatePeer& createPeer,
    const CreatePeer& createRecordPeer) :
    owner(owner),
    iceServers(iceServers),
    createPeer(createPeer),
    createRecordPeer(createRecordPeer)
{
}

void ServerSession::Private::sendIceCandidates(
    const rtsp::SessionId& session,
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

void ServerSession::Private::streamerPrepared(rtsp::CSeq describeRequestCSeq)
{
    auto requestIt = describeRequests.find(describeRequestCSeq);
    if(describeRequests.end() == requestIt ||
       rtsp::Method::DESCRIBE != requestIt->second.requestPtr->method)
    {
        owner->disconnect();
        return;
    }

    AutoEraseRequest autoEraseRequest(this, requestIt);

    RequestInfo& requestInfo = requestIt->second;
    const rtsp::SessionId& session = requestInfo.session;

    auto it = mediaSessions.find(session);
    if(mediaSessions.end() == it || it->second->recorder) {
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
        prepareOkResponse(requestInfo.requestPtr->cseq, session, &response);

        response.headerFields.emplace("Content-Type", "application/sdp");

        response.body = localPeer.sdp();

        owner->sendResponse(response);

        mediaSession.createRequest.swap(requestInfo.requestPtr);

        sendIceCandidates(session, &mediaSession);
    }
}

void ServerSession::Private::recorderPrepared(rtsp::CSeq recordRequestCSeq)
{
    auto requestIt = recordRequests.find(recordRequestCSeq);
    if(recordRequests.end() == requestIt ||
       rtsp::Method::RECORD != requestIt->second.requestPtr->method)
    {
        owner->disconnect();
        return;
    }

    AutoEraseRecordRequest autoEraseRequest(this, requestIt);

    RequestInfo& requestInfo = requestIt->second;
    const rtsp::SessionId& session = requestInfo.session;

    auto it = mediaSessions.find(session);
    if(mediaSessions.end() == it || !it->second->recorder) {
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
        prepareOkResponse(requestInfo.requestPtr->cseq, session, &response);

        response.headerFields.emplace("Content-Type", "application/sdp");

        response.body = recorder.sdp();

        owner->sendResponse(response);

        mediaSession.createRequest.swap(requestInfo.requestPtr);

        sendIceCandidates(session, &mediaSession);
    }
}

void ServerSession::Private::iceCandidate(
    const rtsp::SessionId& session,
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

void ServerSession::Private::eos(const rtsp::SessionId& session)
{
    Log()->trace("Eos. Session: {}", session);

    owner->onEos();
}


ServerSession::ServerSession(
    const IceServers& iceServers,
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::ServerSession(iceServers, sendRequest, sendResponse),
    _p(new Private(this, iceServers, createPeer))
{
}

ServerSession::ServerSession(
    const IceServers& iceServers,
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createRecordPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::ServerSession(iceServers, sendRequest, sendResponse),
    _p(new Private(this, iceServers, createPeer, createRecordPeer))
{
}

ServerSession::~ServerSession()
{
}

bool ServerSession::onConnected(const std::optional<std::string>& authCookie) noexcept
{
    if(!rtsp::ServerSession::onConnected(authCookie))
        return false;

    _p->authCookie = authCookie;

    return true;
}

bool ServerSession::onOptionsRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    rtsp::Response response;
    prepareOkResponse(requestPtr->cseq, rtsp::SessionId(), &response);

    std::string options;
    if(listEnabled())
        options += "LIST, ";

    options += "DESCRIBE, SETUP, PLAY, TEARDOWN";

    if(recordEnabled(requestPtr->uri) && _p->recordEnabled())
        options += ", RECORD";
    if(subscribeEnabled(requestPtr->uri))
        options += ", SUBSCRIBE";

    response.headerFields.emplace("Public", options);

    sendResponse(response);

    return true;
}

bool ServerSession::onDescribeRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(!authorize(requestPtr, _p->authCookie)) {
        Log()->error("DESCRIBE authorize failed for \"{}\"", requestPtr->uri);

        sendUnauthorizedResponse(requestPtr->cseq);

        return true;
    }

    std::unique_ptr<WebRTCPeer> peerPtr = _p->createPeer(requestPtr->uri);
    if(!peerPtr)
        return false;

    const rtsp::SessionId session = _p->nextSession();
    auto requestPair =
        _p->describeRequests.emplace(
            requestPtr->cseq,
            RequestInfo {
                .requestPtr = nullptr,
                .session = session,
            });
    if(!requestPair.second)
        return false;

    RequestInfo& requestInfo = requestPair.first->second;
    requestInfo.requestPtr = std::move(requestPtr);
    rtsp::Request& request = *requestInfo.requestPtr;

    Private::AutoEraseRequest autoEraseRequest(_p.get(), requestPair.first);

    auto emplacePair =
        _p->mediaSessions.emplace(
            session,
            std::make_unique<MediaSession>());
    if(!emplacePair.second)
        return false;

    MediaSession& mediaSession = *(emplacePair.first->second);
    mediaSession.recorder = false;
    mediaSession.uri = request.uri;
    mediaSession.localPeer = std::move(peerPtr);

    mediaSession.localPeer->prepare(
        _p->iceServers,
        std::bind(
            &ServerSession::Private::streamerPrepared,
            _p.get(),
            request.cseq),
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

    autoEraseRequest.discard();

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

bool ServerSession::authorize(
    const std::unique_ptr<rtsp::Request>& requestPtr,
    const std::optional<std::string>&) noexcept
{
    return requestPtr->method != rtsp::Method::RECORD;
}

bool ServerSession::onRecordRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(!recordEnabled(requestPtr->uri) || !_p->recordEnabled())
        return false;

    if(!authorize(requestPtr, _p->authCookie)) {
        Log()->error("RECORD authorize failed for \"{}\"", requestPtr->uri);
        return false;
    }

    std::unique_ptr<WebRTCPeer> peerPtr = _p->createRecordPeer(requestPtr->uri);
    if(!peerPtr)
        return false;

    const std::string contentType = RequestContentType(*requestPtr);
    if(contentType != "application/sdp")
        return false;

    const rtsp::SessionId session = _p->nextSession();
    auto requestPair =
        _p->recordRequests.emplace(
            requestPtr->cseq,
            RequestInfo {
                .requestPtr = nullptr,
                .session = session,
            });
    if(!requestPair.second)
        return false;

    RequestInfo& requestInfo = requestPair.first->second;
    requestInfo.requestPtr = std::move(requestPtr);
    rtsp::Request& request = *requestInfo.requestPtr;

    Private::AutoEraseRecordRequest autoEraseRequest(_p.get(), requestPair.first);

    auto emplacePair =
        _p->mediaSessions.emplace(
            session,
            std::make_unique<MediaSession>());
    if(!emplacePair.second)
        return false;

    MediaSession& mediaSession = *(emplacePair.first->second);
    mediaSession.recorder = true;
    mediaSession.uri = request.uri;
    mediaSession.localPeer = std::move(peerPtr);

    mediaSession.localPeer->prepare(
        _p->iceServers,
        std::bind(
            &ServerSession::Private::recorderPrepared,
            _p.get(),
            request.cseq),
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

    const std::string& sdp = requestInfo.requestPtr->body;
    if(sdp.empty())
        return false;

    mediaSession.localPeer->setRemoteSdp(sdp);

    autoEraseRequest.discard();

    WebRTCPeer& localPeer = *(mediaSession.localPeer);

    localPeer.play();

    return true;
}

bool ServerSession::onSetupRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId session = RequestSession(*requestPtr);

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

            Log()->trace("Adding ice candidate \"{}\"", candidate);

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
    const rtsp::SessionId session = RequestSession(*requestPtr);
    if(session.empty())
        return false;

    auto it = _p->mediaSessions.find(session);
    if(it == _p->mediaSessions.end())
        return false;

    MediaSession& mediaSession = *it->second;
    if(mediaSession.recorder)
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
    const rtsp::SessionId session = RequestSession(*requestPtr);

    auto it = _p->mediaSessions.find(session);
    if(it == _p->mediaSessions.end())
        return false;

    WebRTCPeer& localPeer = *(it->second->localPeer);

    localPeer.stop();

    sendOkResponse(requestPtr->cseq, session);

    _p->mediaSessions.erase(it);

    return true;
}
