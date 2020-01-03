#include "BackSession.h"

#include <cassert>
#include <map>

#include "RtspParser/RtspParser.h"
#include "RtspSession/StatusCode.h"


namespace {

struct RequestSource {
    FrontSession* source;
    rtsp::CSeq sourceCSeq;
    rtsp::SessionId session;
};

typedef std::pair<FrontSession*, rtsp::SessionId> FrontMediaSession;

}

struct BackSession::Private
{
    Private(BackSession* owner, Forwarder*);

    BackSession *const owner;
    Forwarder *const forwarder;

    std::string clientName;

    std::map<rtsp::CSeq, RequestSource> forwardRequests;

    std::map<rtsp::SessionId, FrontMediaSession> mediaSessions;
};

BackSession::Private::Private(
    BackSession* owner, Forwarder* forwarder) :
    owner(owner), forwarder(forwarder)
{
}


BackSession::BackSession(
    Forwarder* forwarder,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::Session(sendRequest, sendResponse),
    _p(new Private(this, forwarder))
{
}

BackSession::~BackSession()
{
    _p->forwarder->removeBackSession(_p->clientName, this);

    for(const auto pair: _p->forwardRequests) {
        const RequestSource& source = pair.second;
        _p->forwarder->cancelRequest(
            source.source, source.sourceCSeq);
    }

    for(const auto pair: _p->mediaSessions) {
        const rtsp::SessionId mediaSessionId = pair.first;
        const FrontMediaSession& mediaSession = pair.second;
        _p->forwarder->dropSession(mediaSession.first);
    }
}

void BackSession::registerMediaSession(
    FrontSession* target,
    const rtsp::SessionId& targetMediaSession,
    const rtsp::SessionId& mediaSession)
{
    _p->mediaSessions.emplace(
        mediaSession,
        FrontMediaSession { target, targetMediaSession });
}

void BackSession::unregisterMediaSession(
    FrontSession* /*target*/,
    const rtsp::SessionId& /*targetMediaSession*/,
    const rtsp::SessionId& mediaSession)
{
    _p->mediaSessions.erase(mediaSession);
}

bool BackSession::handleRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(rtsp::Method::SET_PARAMETER != requestPtr->method && _p->clientName.empty())
        return false;

    return rtsp::Session::handleRequest(requestPtr);
}

bool BackSession::handleSetParameterRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(RequestContentType(*requestPtr) != "text/parameters")
        return false;

    if(!_p->clientName.empty())
        return false;

    rtsp::Parameters parameters;
    if(!rtsp::ParseParameters(requestPtr->body, &parameters))
        return false;

    auto nameIt = parameters.find("name");
    if(parameters.end() == nameIt) {
        return false;
    }
    auto tokenIt = parameters.find("token");
    if(parameters.end() == tokenIt) {
        return false;
    }

    if(!_p->forwarder->registerBackSession(nameIt->second, tokenIt->second, this))
        return false;

    _p->clientName = nameIt->second;

    return true;
}

bool BackSession::handleSetupRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    rtsp::Request& request = *requestPtr;

    const rtsp::SessionId requestMediaSession = RequestSession(request);

    const auto mediaSessionIt = _p->mediaSessions.find(requestMediaSession);
    if(_p->mediaSessions.end() == mediaSessionIt)
        return false;

    const FrontMediaSession& targetMediaSession = mediaSessionIt->second;
    FrontSession* target = targetMediaSession.first;

    rtsp::SetRequestSession(&request, targetMediaSession.second);

    return
        _p->forwarder->forwardToFrontSession(
            this, target, requestPtr);
}

bool BackSession::forward(
    FrontSession* source,
    std::unique_ptr<rtsp::Request>& sourceRequestPtr)
{
    rtsp::Request& sourceRequest = *sourceRequestPtr;
    const rtsp::CSeq sourceCSeq = sourceRequest.cseq;
    const rtsp::SessionId sourceSession = rtsp::RequestSession(sourceRequest);

    rtsp::Request* request =
        createRequest(sourceRequest.method, sourceRequest.uri);
    const rtsp::CSeq requestCSeq = request->cseq;

    const bool inserted =
        _p->forwardRequests.emplace(
            requestCSeq,
            RequestSource { source, sourceCSeq, sourceSession }).second;
    assert(inserted);

    request->headerFields.swap(sourceRequest.headerFields);
    request->body.swap(sourceRequest.body);
    sourceRequestPtr.reset();

    sendRequest(*request);

    return true;
}

bool BackSession::forward(const rtsp::Response& response)
{
    sendResponse(response);

    return true;
}

bool BackSession::handleResponse(
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>& responsePtr) noexcept
{
    const auto requestIt = _p->forwardRequests.find(responsePtr->cseq);
    if(requestIt == _p->forwardRequests.end())
        return false;

    const RequestSource& requestSource = requestIt->second;

    const rtsp::SessionId responseSessionId = ResponseSession(*responsePtr);
    if(rtsp::Method::DESCRIBE == request.method) {
        if(!requestSource.session.empty() || responseSessionId.empty())
            return false;
    } else if(requestSource.session != responseSessionId)
        return false;

    FrontSession* targetSession = requestSource.source;

    std::unique_ptr<rtsp::Response> tmpResponsePtr = std::move(responsePtr);
    tmpResponsePtr->cseq = requestSource.sourceCSeq;

    return
        _p->forwarder->forwardToFrontSession(
            this, targetSession, request, tmpResponsePtr);
}

void BackSession::cancelRequest(const rtsp::CSeq& cseq)
{
    rtsp::Response response;
    prepareResponse(
        rtsp::StatusCode::BAD_GATEWAY, "Bad gateway",
        cseq, rtsp::SessionId(), &response);
    sendResponse(response);
}

void BackSession::forceTeardown(const rtsp::SessionId& mediaSession)
{
    const rtsp::Request* request =
        createRequest(rtsp::Method::TEARDOWN, "*", mediaSession);
    sendRequest(*request);
}
