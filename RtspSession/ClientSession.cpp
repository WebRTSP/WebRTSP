#include "ClientSession.h"


namespace rtsp {

CSeq ClientSession::requestOptions(const std::string& uri) noexcept
{
    rtsp::Request& request =
        *createRequest(rtsp::Method::OPTIONS, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestDescribe(const std::string& uri) noexcept
{
    rtsp::Request& request =
        *createRequest(rtsp::Method::DESCRIBE, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestPlay(
    const std::string& uri,
    const rtsp::SessionId& session) noexcept
{
    rtsp::Request& request =
        *createRequest(rtsp::Method::PLAY, uri, session);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestTeardown(
    const std::string& uri,
    const rtsp::SessionId& session) noexcept
{
    rtsp::Request& request =
        *createRequest(rtsp::Method::TEARDOWN, uri, session);

    sendRequest(request);

    return request.cseq;
}

bool ClientSession::handleResponse(
    const rtsp::Request& request, const rtsp::Response& response) noexcept
{
    switch(request.method) {
        case rtsp::Method::OPTIONS:
            return onOptionsResponse(request, response);
        case rtsp::Method::DESCRIBE:
            return onDescribeResponse(request, response);
        case rtsp::Method::SETUP:
            return onSetupResponse(request, response);
        case rtsp::Method::PLAY:
            return onPlayResponse(request, response);
        case rtsp::Method::TEARDOWN:
            return onTeardownResponse(request, response);
        default:
            return false;
    }
}

}
