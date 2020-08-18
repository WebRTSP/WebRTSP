#include "ServerSession.h"


namespace rtsp {

bool ServerSession::onOptionsRequest(
    std::unique_ptr<Request>&) noexcept
{
    return false;
}

bool ServerSession::onListRequest(
    std::unique_ptr<Request>&) noexcept
{
    return false;
}

bool ServerSession::onDescribeRequest(
    std::unique_ptr<Request>&) noexcept
{
    return false;
}

bool ServerSession::onPlayRequest(
    std::unique_ptr<Request>&) noexcept
{
    return false;
}

bool ServerSession::onTeardownRequest(
    std::unique_ptr<Request>&) noexcept
{
    return false;
}

bool ServerSession::handleRequest(
    std::unique_ptr<Request>& requestPtr) noexcept
{
    switch(requestPtr->method) {
    case Method::OPTIONS:
        return onOptionsRequest(requestPtr);
    case Method::LIST:
        return onListRequest(requestPtr);
    case Method::DESCRIBE:
        return onDescribeRequest(requestPtr);
    case Method::SETUP:
        return onSetupRequest(requestPtr);
    case Method::PLAY:
        return onPlayRequest(requestPtr);
    case Method::TEARDOWN:
        return onTeardownRequest(requestPtr);
    default:
        return Session::handleRequest(requestPtr);
    }
}

}
