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

CSeq ClientSession::requestRecord(
    const std::string& uri,
    const std::string& sdp,
    const std::string& token) noexcept
{
    Request& request =
        *createRequest(Method::RECORD, uri);

    request.headerFields.emplace("Content-Type", "application/sdp");

    if(!token.empty())
        request.headerFields.emplace("Authorization", "Bearer " + token);

    request.body.assign(sdp);

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
