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

    SetContentType(&request, rtsp::SdpContentType);

    if(!token.empty())
        request.headerFields.emplace("Authorization", "Bearer " + token);

    request.body.assign(sdp);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestPlay(
    const std::string& uri,
    const MediaSessionId& session,
    const std::string& sdp) noexcept
{
    Request* request = createRequest(Method::PLAY, uri, session);
    rtsp::SetContentType(request, SdpContentType);
    request->body = sdp;

    sendRequest(*request);

    return request->cseq;
}

CSeq ClientSession::requestTeardown(
    const std::string& uri,
    const MediaSessionId& session) noexcept
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

    if(playSupportRequired(request.uri) &&
        (!isSupported(Method::DESCRIBE) ||
        !isSupported(Method::SETUP) ||
        !isSupported(Method::PLAY) ||
        !isSupported(Method::TEARDOWN)))
    {
        return false;
    }

    if(recordSupportRequired(request.uri) &&
        (!isSupported(Method::RECORD) ||
        !isSupported(Method::SETUP) ||
        !isSupported(Method::TEARDOWN)))
    {
        return false;
    }

    return true;
}

}
