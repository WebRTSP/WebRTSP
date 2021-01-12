#include "ServerSession.h"

#include <list>
#include <map>

#include "RtspSession/StatusCode.h"

#include "GstStreaming/GstTestStreamer.h"

#include "Log.h"


namespace {

struct StreamerInfo
{
    std::string uri;
    std::unique_ptr<rtsp::Request> describeRequest;
    std::unique_ptr<WebRTCPeer> streamer;
};

typedef std::map<rtsp::SessionId, std::unique_ptr<StreamerInfo>> Streamers;

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

    Private(
        ServerSession* owner,
        std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> createPeer);

    ServerSession* owner;
    std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> createPeer;

    std::deque<std::string> iceServers;

    Requests describeRequests;
    Streamers streamers;

    std::string nextSession()
        { return std::to_string(_nextSession++); }

    void streamerPrepared(rtsp::CSeq describeRequestCSeq);
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

ServerSession::Private::Private(
    ServerSession* owner,
    std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> createPeer) :
    owner(owner), createPeer(createPeer)
{
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

    auto it = streamers.find(session);
    if(streamers.end() == it) {
        owner->disconnect();
        return;
    }

    StreamerInfo& streamerInfo = *(it->second);
    WebRTCPeer& streamer = *streamerInfo.streamer;

    std::string sdp;
    if(!streamer.sdp(&sdp))
        owner->disconnect();
    else {
        rtsp::Response response;
        prepareOkResponse(requestInfo.requestPtr->cseq, session, &response);

        response.headerFields.emplace("Content-Type", "application/sdp");

        response.body.swap(sdp);

        owner->sendResponse(response);

        streamerInfo.describeRequest.swap(requestInfo.requestPtr);
    }
}

void ServerSession::Private::iceCandidate(
    const rtsp::SessionId& session,
    unsigned mlineIndex, const std::string& candidate)
{
    auto it = streamers.find(session);
    if(streamers.end() == it) {
        owner->disconnect();
        return;
    }

    const StreamerInfo& streamerInfo = *(it->second);

    owner->requestSetup(
        streamerInfo.uri,
        "application/x-ice-candidate",
        session,
        std::to_string(mlineIndex) + "/" + candidate + "\r\n");
}

void ServerSession::Private::eos(const rtsp::SessionId& session)
{
    Log()->trace("Eos. Session: {}", session);
}


ServerSession::ServerSession(
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::ServerSession(sendRequest, sendResponse),
    _p(new Private(this, createPeer))
{
}

ServerSession::~ServerSession()
{
}

void ServerSession::setIceServers(const WebRTCPeer::IceServers& iceServers)
{
    _p->iceServers = iceServers;
}

bool ServerSession::onOptionsRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    rtsp::Response response;
    prepareOkResponse(requestPtr->cseq, rtsp::SessionId(), &response);

    response.headerFields.emplace("Public", "DESCRIBE, SETUP, PLAY, TEARDOWN");

    sendResponse(response);

    return true;
}

bool ServerSession::onDescribeRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
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

    auto streamerPair =
        _p->streamers.emplace(
            session,
            std::make_unique<StreamerInfo>());
    if(!streamerPair.second)
        return false;

    StreamerInfo& streamerInfo = *(streamerPair.first->second);
    streamerInfo.uri = request.uri;
    streamerInfo.streamer = std::move(peerPtr);

    streamerInfo.streamer->prepare(
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

bool ServerSession::onSetupRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId session = RequestSession(*requestPtr);

    auto it = _p->streamers.find(session);
    if(it == _p->streamers.end())
        return false;

    WebRTCPeer& streamer = *it->second->streamer;

    if(RequestContentType(*requestPtr) == "application/sdp") {
        streamer.setRemoteSdp(requestPtr->body);

        sendOkResponse(requestPtr->cseq, session);

        return true;
    }

    if(RequestContentType(*requestPtr) != "application/x-ice-candidate")
        return false;

    const std::string& ice = requestPtr->body;

    const std::string::size_type delimiterPos = ice.find("/");
    if(delimiterPos == std::string::npos || 0 == delimiterPos)
        return false;

    const std::string::size_type lineEndPos = ice.find("\r\n", delimiterPos + 1);
    if(lineEndPos == std::string::npos)
        return false;

    try{
        int idx = std::stoi(ice.substr(0, delimiterPos - 0));
        if(idx < 0)
            return false;

        const std::string candidate =
            ice.substr(delimiterPos + 1, lineEndPos - (delimiterPos + 1));

        if(candidate.empty())
            return false;

        Log()->trace("Adding ice candidate \"{}\"", candidate);

        streamer.addIceCandidate(idx, candidate);

        sendOkResponse(requestPtr->cseq, session);

        return true;
    } catch(...) {
        return false;
    }
}

bool ServerSession::onPlayRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId session = RequestSession(*requestPtr);

    auto it = _p->streamers.find(session);
    if(it == _p->streamers.end())
        return false;

    WebRTCPeer& streamer = *(it->second->streamer);

    streamer.play();

    sendOkResponse(requestPtr->cseq, session);

    return true;
}

bool ServerSession::onTeardownRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId session = RequestSession(*requestPtr);

    auto it = _p->streamers.find(session);
    if(it == _p->streamers.end())
        return false;

    WebRTCPeer& streamer = *(it->second->streamer);

    streamer.stop();

    sendOkResponse(requestPtr->cseq, session);

    _p->streamers.erase(it);

    return true;
}
