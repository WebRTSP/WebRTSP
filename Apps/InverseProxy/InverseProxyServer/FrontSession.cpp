#include "FrontSession.h"

#include <list>
#include <map>

#include "RtspSession/StatusCode.h"

#include "GstStreaming/GstTestStreamer.h"


struct FrontSession::Private
{
    Private(FrontSession* owner, ForwardContext*);

    FrontSession *const owner;
    ForwardContext *const forwardContext;
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
    _p->forwardContext->registerFrontSession(this);
}

FrontSession::~FrontSession()
{
    _p->forwardContext->removeFrontSession(this);
}

bool FrontSession::handleRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    return _p->forwardContext->forwardToBackSession(this, requestPtr);
}

rtsp::CSeq FrontSession::forward(const rtsp::Request& request)
{
    rtsp::Request* outRequest = createRequest(request.method, request.uri);

    outRequest->headerFields = request.headerFields;
    outRequest->body = request.body;

    sendRequest(*outRequest);

    return outRequest->cseq;
}

void FrontSession::forward(
    const rtsp::Request& /*request*/,
    const rtsp::Response& response)
{
    sendResponse(response);
}

bool FrontSession::handleResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    return _p->forwardContext->forwardToBackSession(this, request, response);
}
