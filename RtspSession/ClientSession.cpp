#include "ClientSession.h"

#include <cassert>

#include "RtspParser/RtspParser.h"


namespace rtsp {

bool ClientSession::isSupported(Method method)
{
    return _supportedMethods.find(method) != _supportedMethods.end();
}

CSeq ClientSession::requestOptions(const std::string& uri) noexcept
{
    assert(!uri.empty());
    if(uri.empty())
        return 0;

    Request& request =
        *createRequest(Method::OPTIONS, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestList() noexcept
{
    Request& request =
        *createRequest(rtsp::Method::LIST, "*");

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
        case Method::LIST:
            return onListResponse(request, *responsePtr);
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

bool ClientSession::onOptionsResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    _supportedMethods = rtsp::ParseOptions(response);

    return
        isSupported(Method::DESCRIBE) &&
        isSupported(Method::SETUP) &&
        isSupported(Method::PLAY) &&
        isSupported(Method::TEARDOWN);
}

}
