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
    rtsp::ServerSession(sendRequest, sendResponse),
    _p(new Private(this, forwardContext))
{
}

FrontSession::~FrontSession()
{
}

bool FrontSession::handleOptionsRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    return false;
}

bool FrontSession::handleDescribeRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    return false;
}

bool FrontSession::handleSetupRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId session = RequestSession(*requestPtr);

    return false;
}

bool FrontSession::handlePlayRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId session = RequestSession(*requestPtr);

    return false;
}

bool FrontSession::handleTeardownRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId session = RequestSession(*requestPtr);

    return false;
}
