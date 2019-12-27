#include "BackSession.h"

#include <cassert>
#include <map>

#include "RtspSession/StatusCode.h"


namespace {

struct RequestSource {
    FrontSession* source;
    rtsp::CSeq sourceCSeq;
    rtsp::SessionId session;
};

}

struct BackSession::Private
{
    Private(BackSession* owner, ForwardContext*);

    BackSession *const owner;
    ForwardContext *const forwardContext;

    std::string clientName;

    std::map<rtsp::CSeq, RequestSource> forwardRequests;
    std::map<rtsp::SessionId, FrontSession*> mediaSessions;
};

BackSession::Private::Private(
    BackSession* owner, ForwardContext* forwardContext) :
    owner(owner), forwardContext(forwardContext)
{
}


BackSession::BackSession(
    ForwardContext* forwardContext,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::Session(sendRequest, sendResponse),
    _p(new Private(this, forwardContext))
{
}

BackSession::~BackSession()
{
    _p->forwardContext->removeBackSession(_p->clientName, this);
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

    const std::string& parameters = requestPtr->body;

    const std::string::size_type delimiterPos = parameters.find(":");
    if(delimiterPos == std::string::npos || 0 == delimiterPos)
        return false;

    const std::string::size_type lineEndPos = parameters.find("\r\n", delimiterPos + 1);
    if(lineEndPos == std::string::npos)
        return false;

    const std::string name = parameters.substr(0, delimiterPos - 0);

    const std::string value =
        parameters.substr(delimiterPos + 1, lineEndPos - (delimiterPos + 1));

    if(name != "name")
        return false;

    if(!_p->clientName.empty())
        return false;

    if(!_p->forwardContext->registerBackSession(value, this))
        return false;

    _p->clientName = value;

    return true;
}

bool BackSession::handleSetupRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    rtsp::Request& request = *requestPtr;

    const rtsp::SessionId sessionId = RequestSession(request);

    const auto mediaSessionIt = _p->mediaSessions.find(sessionId);
    if(_p->mediaSessions.end() == mediaSessionIt)
        return false;

    FrontSession* targetSession = mediaSessionIt->second;

    return
        _p->forwardContext->forwardToFrontSession(
            this, targetSession, requestPtr);
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

bool BackSession::manageMediaSessions(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if((rtsp::Method::DESCRIBE != request.method ||
        rtsp::Method::TEARDOWN != request.method) &&
       rtsp::StatusCode::OK != response.statusCode)
    {
        return true;
    }

    const auto requestsIt = _p->forwardRequests.find(response.cseq);
    if(requestsIt == _p->forwardRequests.end())
        return false;

    const RequestSource& requestSource = requestsIt->second;
    FrontSession* requestSourceSession = requestSource.source;

    if(rtsp::Method::DESCRIBE == request.method) {
        const rtsp::SessionId sessionId = ResponseSession(response);
        const bool inserted =
            _p->mediaSessions.emplace(sessionId, requestSourceSession).second;

        if(!inserted)
            return false;
    } else if(rtsp::Method::TEARDOWN == request.method) {
        const rtsp::SessionId sessionId = ResponseSession(response);
        const bool erased =
            _p->mediaSessions.erase(sessionId) > 0;

        if(!erased)
            return false;
    }

    return true;
}

bool BackSession::handleResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(!manageMediaSessions(request, response))
        return false;

    const auto requestIt = _p->forwardRequests.find(response.cseq);
    if(requestIt == _p->forwardRequests.end())
        return false;

    const RequestSource& requestSource = requestIt->second;

    const rtsp::SessionId responseSessionId = ResponseSession(response);
    if(rtsp::Method::DESCRIBE == request.method) {
        if(!requestSource.session.empty() || responseSessionId.empty())
            return false;
    } else if(requestSource.session != responseSessionId)
        return false;

    FrontSession* targetSession = requestSource.source;

    rtsp::Response tmpResponse = response;
    tmpResponse.cseq = requestSource.sourceCSeq;

    return _p->forwardContext->forwardToFrontSession(targetSession, tmpResponse);
}
