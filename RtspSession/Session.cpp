#include "Session.h"


namespace rtsp {

Session::Session(
    const std::function<void (const Request*)>& sendRequest,
    const std::function<void (const Response*)>& sendResponse) noexcept :
    _sendRequest(sendRequest), _sendResponse(sendResponse)
{
}

Request* Session::createRequest(
    Method method,
    const std::string& uri) noexcept
{
    for(;;) {
        const auto& pair =
            _sentRequests.emplace(
                _nextCSeq,
                Request{
                    .method = method,
                    .protocol = Protocol::WEBRTSP_0_1,
                    .cseq = _nextCSeq
                });

        ++_nextCSeq;

        if(pair.second) {
            Request& request = pair.first->second;
            request.uri = uri;
            return &request;
        }
    }
}

Request* Session::createRequest(
    Method method,
    const std::string& uri,
    const SessionId& session) noexcept
{
    Request* request = createRequest(method, uri);

    request->headerFields.emplace("Session", session);

    return request;
}

bool Session::handleRequest(std::unique_ptr<Request>& requestPtr) noexcept
{
    switch(requestPtr->method) {
    case rtsp::Method::SETUP:
        return handleSetupRequest(requestPtr);
    default:
        return false;
    }
}

Response* Session::prepareResponse(
    StatusCode statusCode,
    const std::string::value_type* reasonPhrase,
    CSeq cseq,
    const SessionId& session,
    Response* out)
{
    out->protocol = Protocol::WEBRTSP_0_1;
    out->cseq = cseq;
    out->statusCode = statusCode;
    out->reasonPhrase = reasonPhrase;

    if(!session.empty())
        out->headerFields.emplace("Session", session);

    return out;
}

Response* Session::prepareOkResponse(
    CSeq cseq,
    const SessionId& session,
    Response* out)
{
    return prepareResponse(OK, "OK", cseq, session, out);
}

void Session::sendOkResponse(
    rtsp::CSeq cseq,
    const rtsp::SessionId& session)
{
    rtsp::Response response;
    sendResponse(*prepareOkResponse(cseq, session, &response));
}

void Session::sendRequest(const Request& request) noexcept
{
    _sendRequest(&request);
}

CSeq Session::requestSetup(
    const std::string& uri,
    const std::string& contentType,
    const rtsp::SessionId& session,
    const std::string& body) noexcept
{
    rtsp::Request& request =
        *createRequest(rtsp::Method::SETUP, uri);

    request.headerFields.emplace("Session", session);
    request.headerFields.emplace("content-type", contentType);

    request.body = body;

    sendRequest(request);

    return request.cseq;
}

void Session::sendResponse(const Response& response) noexcept
{
    _sendResponse(&response);
}

void Session::disconnect() noexcept
{
    _sendRequest(nullptr);
}

bool Session::handleResponse(const Response& response) noexcept
{
    auto it = _sentRequests.find(response.cseq);
    if(it == _sentRequests.end())
        return false;

    const Request& request = it->second;
    return handleResponse(request, response);
}

bool Session::handleResponse(const Request& request, const Response& response) noexcept
{
    switch(request.method) {
    case rtsp::Method::SETUP:
        return handleSetupResponse(request, response);
    default:
        return false;
    }
}

bool Session::handleSetupResponse(const Request& request, const Response& response) noexcept
{
    if(StatusCode::OK == response.statusCode)
        return true;

    return false;
}

}
