#include "BackSession.h"

#include "RtspSession/StatusCode.h"


struct BackSession::Private
{
    Private(BackSession* owner, ForwardContext*);

    BackSession *const owner;
    ForwardContext *const forwardContext;
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
    rtsp::ClientSession(sendRequest, sendResponse),
    _p(new Private(this, forwardContext))
{
}

BackSession::~BackSession()
{
}

bool BackSession::onOptionsResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    return false;
}

bool BackSession::onDescribeResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    return false;
}

bool BackSession::onSetupResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    return false;
}

bool BackSession::onPlayResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    return false;
}

bool BackSession::onTeardownResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    return false;
}

bool BackSession::handleSetupRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    return false;
}

