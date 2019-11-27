#include "ServerSession.h"

#include <list>
#include <map>

#include "RtspSession/StatusCode.h"


namespace {

struct StreamerInfo
{
    std::string uri;
    streaming::GstStreamer streamer;
};

typedef std::map<rtsp::SessionId, std::unique_ptr<StreamerInfo>> Streamers;

struct RequestInfo
{
    std::unique_ptr<rtsp::Request> requestPtr;
    const rtsp::SessionId session;
};

typedef std::map<rtsp::CSeq, RequestInfo> Requests;

rtsp::SessionId RequestSession(const rtsp::Request& request)
{
    auto it = request.headerFields.find("session");
    if(request.headerFields.end() == it)
        return rtsp::SessionId();

    return it->second;
}

}

struct ServerSession::Private
{
    ServerSession* owner;

    unsigned _nextSession = 1;

    Requests requests;
    Streamers streamers;

    struct AutoEraseRequest;

    std::string nextSession()
        { return std::to_string(_nextSession++); }

    void streamerPrepared(rtsp::CSeq describeRequestCSeq);
};

struct ServerSession::Private::AutoEraseRequest
{
    AutoEraseRequest(
        ServerSession::Private* owner,
        Requests::const_iterator it) :
        _owner(owner), _it(it) {}
    ~AutoEraseRequest()
        { if(_owner) _owner->requests.erase(_it); }
    void discard()
        { _owner = nullptr; }

private:
    ServerSession::Private* _owner;
    Requests::const_iterator _it;
};

void ServerSession::Private::streamerPrepared(rtsp::CSeq describeRequestCSeq)
{
    using streaming::GstStreamer;

    auto requestIt = requests.find(describeRequestCSeq);
    if(requests.end() == requestIt ||
       rtsp::Method::DESCRIBE != requestIt->second.requestPtr->method)
    {
        owner->disconnect();
        return;
    }

    AutoEraseRequest autoEraseRequest(this, requestIt);

    const RequestInfo& requestInfo = requestIt->second;
    const rtsp::SessionId& session = requestInfo.session;

    if(streamers.end() == streamers.find(session)) {
        owner->disconnect();
        return;
    };

    auto it = streamers.find(session);
    if(streamers.end() == it) {
        owner->disconnect();
        return;
    }

    StreamerInfo& streamerInfo = *(it->second);
    GstStreamer& streamer = streamerInfo.streamer;

    std::string sdp;
    if(!streamer.sdp(&sdp))
        owner->disconnect();
    else {
        rtsp::Response response;
        prepareOkResponse(requestInfo.requestPtr->cseq, session, &response);

        response.headerFields.emplace("Content-Type", "application/sdp");

        response.body.swap(sdp);

        owner->sendResponse(response);
    }
}

ServerSession::ServerSession(
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::ServerSession(sendRequest, sendResponse),
    _p(new Private { .owner = this })
{
}

ServerSession::~ServerSession()
{
}

bool ServerSession::handleOptionsRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    rtsp::Response response;
    prepareOkResponse(requestPtr->cseq, rtsp::SessionId(), &response);

    response.headerFields.emplace("Public", "DESCRIBE, SETUP, PLAY, TEARDOWN");

    sendResponse(response);

    return true;
}

bool ServerSession::handleDescribeRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    using streaming::GstStreamer;

    const rtsp::SessionId session = _p->nextSession();
    auto requestPair =
        _p->requests.emplace(
            requestPtr->cseq,
            RequestInfo {
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

    streamerInfo.streamer.prepare(
        std::bind(
            &ServerSession::Private::streamerPrepared,
            _p.get(),
            request.cseq));

    autoEraseRequest.discard();

    return true;
}

bool ServerSession::handleSetupRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId session = RequestSession(*requestPtr);

    auto it = _p->streamers.find(session);
    if(it == _p->streamers.end())
        return false;

    using streaming::GstStreamer;
    GstStreamer& streamer = it->second->streamer;

    streamer.setRemoteSdp(requestPtr->body);

    sendOkResponse(requestPtr->cseq, session);

    return true;
}

bool ServerSession::handlePlayRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId session = RequestSession(*requestPtr);

    auto it = _p->streamers.find(session);
    if(it == _p->streamers.end())
        return false;

    using streaming::GstStreamer;
    GstStreamer& streamer = it->second->streamer;

    streamer.play();

    sendOkResponse(requestPtr->cseq, session);

    return true;
}

bool ServerSession::handleTeardownRequest(
    std::unique_ptr<rtsp::Request>&) noexcept
{
    return false;
}
