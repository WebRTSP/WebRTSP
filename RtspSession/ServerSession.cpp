#include "ServerSession.h"


namespace rtsp {

bool ServerSession::handleOptionsRequest(
    std::unique_ptr<Request>&) noexcept
{
    return false;
}

bool ServerSession::handleDescribeRequest(
    std::unique_ptr<Request>&) noexcept
{
    return false;
}

bool ServerSession::handlePlayRequest(
    std::unique_ptr<Request>&) noexcept
{
    return false;
}

bool ServerSession::handleTeardownRequest(
    std::unique_ptr<Request>&) noexcept
{
    return false;
}

bool ServerSession::handleRequest(
    std::unique_ptr<Request>& requestPtr) noexcept
{
    switch(requestPtr->method) {
    case Method::OPTIONS:
        return handleOptionsRequest(requestPtr);
    case Method::DESCRIBE:
        return handleDescribeRequest(requestPtr);
    case Method::SETUP:
        return handleSetupRequest(requestPtr);
    case Method::PLAY:
        return handlePlayRequest(requestPtr);
    case Method::TEARDOWN:
        return handleTeardownRequest(requestPtr);
    default:
        return Session::handleRequest(requestPtr);
    }
}

}
