#include "ServerSession.h"

#include <list>
#include <map>

#include "RtspSession/StatusCode.h"


namespace {

typedef std::map<std::string, std::unique_ptr<streaming::GstStreamer> > Streamers;

struct Request
{
    std::unique_ptr<rtsp::Request> requestPtr;
    Streamers::iterator streamerIt;
};

typedef std::list<Request> Requests;

}

struct ServerSession::Private
{
    ServerSession* owner;

    Requests requests;
    Streamers streamers;

    rtsp::Response* prepareResponse(
        rtsp::CSeq cseq,
        rtsp::StatusCode statusCode,
        const std::string::value_type* reasonPhrase,
        rtsp::Response* out);
    rtsp::Response* prepareOkResponse(
        rtsp::CSeq cseq,
        rtsp::Response* out);
    void sendOkResponse(rtsp::CSeq cseq);

    void streamerPrepared(const Requests::iterator& it);
};

rtsp::Response* ServerSession::Private::prepareResponse(
    rtsp::CSeq cseq,
    rtsp::StatusCode statusCode,
    const std::string::value_type* reasonPhrase,
    rtsp::Response* out)
{
    out->protocol = rtsp::Protocol::RTSP_1_0;
    out->cseq = cseq;
    out->statusCode = statusCode;
    out->reasonPhrase = reasonPhrase;

    return out;
}

rtsp::Response* ServerSession::Private::prepareOkResponse(
    rtsp::CSeq cseq,
    rtsp::Response* out)
{
    return prepareResponse(cseq, rtsp::OK, "OK", out);
}

void ServerSession::Private::sendOkResponse(rtsp::CSeq cseq)
{
    rtsp::Response response;
    owner->sendResponse(prepareOkResponse(cseq, &response));
}

void ServerSession::Private::streamerPrepared(const Requests::iterator& it)
{
    using streaming::GstStreamer;

    const Request& request = *it;
    GstStreamer& streamer = *request.streamerIt->second;

    std::string sdp;
    if(!streamer.sdp(&sdp)) {
        owner->sendResponse(nullptr);
    } else {
        rtsp::Response response;
        prepareOkResponse(request.requestPtr->cseq, &response);

        response.headerFields.emplace("Content-Base", request.requestPtr->uri);
        response.headerFields.emplace("Content-Type", "application/sdp");

        response.body.swap(sdp);

        owner->sendResponse(&response);
    }

    requests.erase(it);
}

ServerSession::ServerSession(
    const std::function<void (const rtsp::Response*)>& sendResponse) :
    rtsp::ServerSession(sendResponse), _p(new Private { .owner = this })
{
}

ServerSession::~ServerSession()
{
}

bool ServerSession::handleOptionsRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    rtsp::Response response;
    _p->prepareOkResponse(requestPtr->cseq, &response);

    response.headerFields.emplace("Public", "DESCRIBE, SETUP, PLAY, TEARDOWN");

    sendResponse(&response);

    return true;
}

bool ServerSession::handleDescribeRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    using streaming::GstStreamer;

    auto pair =
        _p->streamers.emplace(requestPtr->uri, std::make_unique<GstStreamer>());
    if(!pair.second)
        return false;

    auto requestIterator =
        _p->requests.emplace(
            _p->requests.end(),
            Request {
                .requestPtr = std::move(requestPtr),
                .streamerIt = pair.first,
            });

    std::unique_ptr<GstStreamer>& streamerPtr = pair.first->second;
    streamerPtr->prepare(
        std::bind(&ServerSession::Private::streamerPrepared, _p.get(), requestIterator));

    return true;
}

bool ServerSession::handleSetupRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    auto it = _p->streamers.find(requestPtr->uri);
    if(it == _p->streamers.end())
        return false;

    using streaming::GstStreamer;
    GstStreamer* streamer = it->second.get();

    streamer->setRemoteSdp(requestPtr->body);

    _p->sendOkResponse(requestPtr->cseq);

    return true;
}

bool ServerSession::handlePlayRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    auto it = _p->streamers.find(requestPtr->uri);
    if(it == _p->streamers.end())
        return false;

    using streaming::GstStreamer;
    GstStreamer* streamer = it->second.get();

    streamer->play();

    _p->sendOkResponse(requestPtr->cseq);

    return true;
}

bool ServerSession::handleTeardownRequest(
    std::unique_ptr<rtsp::Request>&) noexcept
{
    return false;
}
