#include "ClientSession.h"


namespace rtsp {

CSeq ClientSession::requestOptions(const std::string& uri) noexcept
{
    Request& request =
        *createRequest(Method::OPTIONS, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestDescribe(const std::string& uri) noexcept
{
    Request& request =
        *createRequest(Method::DESCRIBE, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestPlay(
    const std::string& uri,
    const SessionId& session) noexcept
{
    Request& request =
        *createRequest(Method::PLAY, uri, session);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestTeardown(
    const std::string& uri,
    const SessionId& session) noexcept
{
    Request& request =
        *createRequest(Method::TEARDOWN, uri, session);

    sendRequest(request);

    return request.cseq;
}

bool ClientSession::handleResponse(
    const Request& request,
    std::unique_ptr<Response>& responsePtr) noexcept
{
    switch(request.method) {
        case Method::OPTIONS:
            return onOptionsResponse(request, *responsePtr);
        case Method::DESCRIBE:
            return onDescribeResponse(request, *responsePtr);
        case Method::PLAY:
            return onPlayResponse(request, *responsePtr);
        case Method::TEARDOWN:
            return onTeardownResponse(request, *responsePtr);
        default:
            return Session::handleResponse(request, responsePtr);
    }
}

}
