#include "ServerSession.h"


namespace rtsp {

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
    case Method::RECORD:
        return onRecordRequest(requestPtr);
    case Method::TEARDOWN:
        return onTeardownRequest(requestPtr);
    default:
        return Session::handleRequest(requestPtr);
    }
}

}
