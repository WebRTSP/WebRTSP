#include "ClientSession.h"


namespace rtsp {

ClientSession::ClientSession(
    const std::function<void (const rtsp::Request*)>& cb) noexcept :
    _requestCallback(cb)
{
}

void ClientSession::sendRequest(const rtsp::Request& request) noexcept
{
    _requestCallback(&request);
}

rtsp::Request* ClientSession::createRequest(
    rtsp::Method method,
    const std::string& uri) noexcept
{
    for(;;) {
        const auto& pair =
            _sentRequests.emplace(
                _nextCSeq,
                rtsp::Request{
                    .method = method,
                    .protocol = Protocol::RTSP_1_0,
                    .cseq = _nextCSeq
                });

        ++_nextCSeq;

        if(pair.second) {
            rtsp::Request& request = pair.first->second;
            request.uri = uri;
            return &request;
        }
    }
}

rtsp::Request* ClientSession::createRequest(
    rtsp::Method method,
    const std::string& uri,
    const std::string& session) noexcept
{
    rtsp::Request* request = createRequest(method, uri);

    request->headerFields.emplace("Session", session);

    return request;
}

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

CSeq ClientSession::requestSetup(const std::string& uri, const std::string& sdp) noexcept
{
    rtsp::Request& request =
        *createRequest(rtsp::Method::SETUP, uri);

    request.body = sdp;

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestPlay(
    const std::string& uri,
    const std::string& session) noexcept
{
    rtsp::Request& request =
        *createRequest(rtsp::Method::PLAY, uri, session);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestTeardown(
    const std::string& uri,
    const std::string& session) noexcept
{
    rtsp::Request& request =
        *createRequest(rtsp::Method::TEARDOWN, uri, session);

    sendRequest(request);

    return request.cseq;
}

bool ClientSession::handleResponse(const rtsp::Response& response) noexcept
{
    auto it = _sentRequests.find(response.cseq);
    if(it == _sentRequests.end())
        return false;

    const Request& request = it->second;
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
