#include "ServerSession.h"


namespace rtsp {

ServerSession::ServerSession(
    const std::function<void (const rtsp::Response*)>& cb) :
    _responseCallback(cb)
{
}

void ServerSession::sendResponse(const rtsp::Response& response)
{
    _responseCallback(&response);
}

bool ServerSession::handleOptionsRequest(
    const rtsp::Request& request)
{
    return false;
}

bool ServerSession::handleDescribeRequest(
    const rtsp::Request& request)
{
    return false;
}

bool ServerSession::handleSetupRequest(
    const rtsp::Request& request)
{
    return false;
}

bool ServerSession::handlePlayRequest(
    const rtsp::Request& request)
{
    return false;
}

bool ServerSession::handleTeardownRequest(
    const rtsp::Request& request)
{
    return false;
}

bool ServerSession::handleRequest(
    const rtsp::Request& request)
{
    switch(request.method) {
    case rtsp::Method::OPTIONS:
        return handleOptionsRequest(request);
    case rtsp::Method::DESCRIBE:
        return handleDescribeRequest(request);
    case rtsp::Method::SETUP:
        return handleRequest(request);
    case rtsp::Method::PLAY:
        return handlePlayRequest(request);
    case rtsp::Method::TEARDOWN:
        return handleTeardownRequest(request);
    default:
        return false;
    }
}

}
