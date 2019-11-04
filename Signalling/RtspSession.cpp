#include "RtspSession.h"


RtspSession::RtspSession(const std::function<void (rtsp::Response*)>& cb) :
    _responseCallback(cb)
{
}

bool RtspSession::handleOptionsRequest(
    const rtsp::Request& request)
{
    auto it = request.headerFields.find("cseq");
    if(request.headerFields.end() == it || it->second.empty())
        return false;

    rtsp::Response response;
    response.protocol = rtsp::Protocol::RTSP_1_0;
    response.headerFields.emplace("CSeq", it->second);
    response.headerFields.emplace("Public", "DESCRIBE, SETUP, PLAY, TEARDOWN");
    response.statusCode = 200;
    response.reasonPhrase = "OK";

    _responseCallback(&response);

    return true;
}

bool RtspSession::handleDescribeRequest(
    const rtsp::Request& request)
{
    return false;
}

bool RtspSession::handleSetupRequest(
    const rtsp::Request& request)
{
    return false;
}

bool RtspSession::handlePlayRequest(
    const rtsp::Request& request)
{
    return false;
}

bool RtspSession::handleTeardownRequest(
    const rtsp::Request& request)
{
    return false;
}

bool RtspSession::handleRequest(
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
