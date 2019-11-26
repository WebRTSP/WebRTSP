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

typedef std::map<rtsp::Session, std::unique_ptr<StreamerInfo>> Streamers;

struct RequestInfo
{
    std::unique_ptr<rtsp::Request> requestPtr;
    const rtsp::Session session;
};

typedef std::map<rtsp::CSeq, RequestInfo> Requests;

rtsp::Session RequestSession(const rtsp::Request& request)
{
    auto it = request.headerFields.find("session");
    if(request.headerFields.end() == it)
        return rtsp::Session();

    return it->second;
}

rtsp::Response* PrepareResponse(
    rtsp::CSeq cseq,
    rtsp::StatusCode statusCode,
    const std::string::value_type* reasonPhrase,
    const rtsp::Session& session,
    rtsp::Response* out)
{
    out->protocol = rtsp::Protocol::RTSP_1_0;
    out->cseq = cseq;
    out->statusCode = statusCode;
    out->reasonPhrase = reasonPhrase;

    if(!session.empty())
        out->headerFields.emplace("session", session);

    return out;
}

rtsp::Response* PrepareOkResponse(
    rtsp::CSeq cseq,
    const rtsp::Session& session,
    rtsp::Response* out)
{
    return PrepareResponse(cseq, rtsp::OK, "OK", session, out);
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

    void sendOkResponse(rtsp::CSeq, const rtsp::Session&);

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

void ServerSession::Private::sendOkResponse(
    rtsp::CSeq cseq,
    const rtsp::Session& session)
{
    rtsp::Response response;
    owner->sendResponse(PrepareOkResponse(cseq, session, &response));
}

void ServerSession::Private::streamerPrepared(rtsp::CSeq describeRequestCSeq)
{
    using streaming::GstStreamer;

    auto requestIt = requests.find(describeRequestCSeq);
    if(requests.end() == requestIt ||
       rtsp::Method::DESCRIBE != requestIt->second.requestPtr->method)
    {
        owner->sendResponse(nullptr);
        return;
    }

    AutoEraseRequest autoEraseRequest(this, requestIt);

    const RequestInfo& requestInfo = requestIt->second;
    const rtsp::Session& session = requestInfo.session;

    if(streamers.end() == streamers.find(session)) {
        owner->sendResponse(nullptr);
        return;
    };

    auto it = streamers.find(session);
    if(streamers.end() == it) {
        owner->sendResponse(nullptr);
        return;
    }

    StreamerInfo& streamerInfo = *(it->second);
    GstStreamer& streamer = streamerInfo.streamer;

    std::string sdp;
    if(!streamer.sdp(&sdp))
        owner->sendResponse(nullptr);
    else {
        rtsp::Response response;
        PrepareOkResponse(requestInfo.requestPtr->cseq, session, &response);

        response.headerFields.emplace("Content-Type", "application/sdp");

        response.body.swap(sdp);

        owner->sendResponse(&response);
    }
}

ServerSession::ServerSession(
    const std::function<void (const rtsp::Response*)>& sendResponse) :
    rtsp::ServerSession(sendResponse),
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
    PrepareOkResponse(requestPtr->cseq, rtsp::Session(), &response);

    response.headerFields.emplace("Public", "DESCRIBE, SETUP, PLAY, TEARDOWN");

    sendResponse(&response);

    return true;
}

bool ServerSession::handleDescribeRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    using streaming::GstStreamer;

    const rtsp::Session session = _p->nextSession();
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
    const rtsp::Session session = RequestSession(*requestPtr);

    auto it = _p->streamers.find(session);
    if(it == _p->streamers.end())
        return false;

    using streaming::GstStreamer;
    GstStreamer& streamer = it->second->streamer;

    streamer.setRemoteSdp(requestPtr->body);

    _p->sendOkResponse(requestPtr->cseq, session);

    return true;
}

bool ServerSession::handlePlayRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::Session session = RequestSession(*requestPtr);

    auto it = _p->streamers.find(session);
    if(it == _p->streamers.end())
        return false;

    using streaming::GstStreamer;
    GstStreamer& streamer = it->second->streamer;

    streamer.play();

    _p->sendOkResponse(requestPtr->cseq, session);

    return true;
}

bool ServerSession::handleTeardownRequest(
    std::unique_ptr<rtsp::Request>&) noexcept
{
    return false;
}
