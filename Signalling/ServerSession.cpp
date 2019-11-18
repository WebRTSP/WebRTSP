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

    void streamerPrepared(const Requests::iterator& it);
};

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
        response.protocol = rtsp::Protocol::RTSP_1_0;
        response.cseq = request.requestPtr->cseq;
        response.statusCode = rtsp::OK;
        response.reasonPhrase = "OK";

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
    response.protocol = rtsp::Protocol::RTSP_1_0;
    response.cseq = requestPtr->cseq;
    response.statusCode = rtsp::OK;
    response.reasonPhrase = "OK";

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
    if(pair.second) {
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

    return false;
}

bool ServerSession::handleSetupRequest(
    std::unique_ptr<rtsp::Request>&) noexcept
{
    return false;
}

bool ServerSession::handlePlayRequest(
    std::unique_ptr<rtsp::Request>&) noexcept
{
    return false;
}

bool ServerSession::handleTeardownRequest(
    std::unique_ptr<rtsp::Request>&) noexcept
{
    return false;
}
