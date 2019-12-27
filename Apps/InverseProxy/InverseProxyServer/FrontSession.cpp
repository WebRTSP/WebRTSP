#include "FrontSession.h"

#include <cassert>
#include <map>

#include "RtspSession/StatusCode.h"

#include "GstStreaming/GstTestStreamer.h"

namespace {

struct RequestSource {
    BackSession* source;
    rtsp::CSeq sourceCSeq;
    rtsp::SessionId session;
};

}

struct FrontSession::Private
{
    Private(FrontSession* owner, ForwardContext*);

    FrontSession *const owner;
    ForwardContext *const forwardContext;

    std::map<rtsp::CSeq, RequestSource> forwardRequests;
};

FrontSession::Private::Private(
    FrontSession* owner, ForwardContext* forwardContext) :
    owner(owner), forwardContext(forwardContext)
{
}


FrontSession::FrontSession(
    ForwardContext* forwardContext,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::Session(sendRequest, sendResponse),
    _p(new Private(this, forwardContext))
{
}

FrontSession::~FrontSession()
{
}

bool FrontSession::handleRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    return _p->forwardContext->forwardToBackSession(this, requestPtr);
}

bool FrontSession::forward(
    BackSession* source,
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

bool FrontSession::forward(const rtsp::Response& response)
{
    sendResponse(response);

    return true;
}

bool FrontSession::handleResponse(
    const rtsp::Request& /*request*/,
    const rtsp::Response& response) noexcept
{
    const auto requestIt = _p->forwardRequests.find(response.cseq);
    if(requestIt == _p->forwardRequests.end())
        return false;

    const RequestSource& requestSource = requestIt->second;

    const rtsp::SessionId responseSessionId = ResponseSession(response);

    if(requestSource.session != responseSessionId)
        return false;

    BackSession* targetSession = requestSource.source;

    rtsp::Response tmpResponse = response;
    tmpResponse.cseq = requestSource.sourceCSeq;

    return _p->forwardContext->forwardToBackSession(targetSession, tmpResponse);
}
