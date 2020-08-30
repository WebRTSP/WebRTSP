#include "Session.h"

#include <cassert>


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
                Request {
                    .method = method,
                    .uri = {},
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
    case Method::SETUP:
        return onSetupRequest(requestPtr);
    case Method::GET_PARAMETER:
        return onGetParameterRequest(requestPtr);
    case Method::SET_PARAMETER:
        return onSetParameterRequest(requestPtr);
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
    Response* out)
{
    return prepareResponse(OK, "OK", cseq, std::string(), out);
}

Response* Session::prepareOkResponse(
    CSeq cseq,
    const SessionId& session,
    Response* out)
{
    return prepareResponse(OK, "OK", cseq, session, out);
}

void Session::sendOkResponse(
    CSeq cseq,
    const SessionId& session)
{
    Response response;
    sendResponse(*prepareOkResponse(cseq, session, &response));
}

void Session::sendOkResponse(
    CSeq cseq,
    const std::string& contentType,
    const std::string& body)
{
    Response response;
    prepareOkResponse(cseq, &response);

    response.headerFields.emplace("content-type", contentType);

    response.body = body;

    sendResponse(response);
}

void Session::sendOkResponse(
    CSeq cseq,
    const SessionId& session,
    const std::string& contentType,
    const std::string& body)
{
    Response response;
    prepareOkResponse(cseq, session, &response);

    response.headerFields.emplace("content-type", contentType);

    response.body = body;

    sendResponse(response);
}

void Session::sendRequest(const Request& request) noexcept
{
    _sendRequest(&request);
}

CSeq Session::requestSetup(
    const std::string& uri,
    const std::string& contentType,
    const SessionId& session,
    const std::string& body) noexcept
{
    assert(!uri.empty());

    Request& request =
        *createRequest(Method::SETUP, uri);

    request.headerFields.emplace("Session", session);
    request.headerFields.emplace("content-type", contentType);

    request.body = body;

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestGetParameter(
    const std::string& uri,
    const std::string& contentType,
    const std::string& body) noexcept
{
    Request& request =
        *createRequest(Method::GET_PARAMETER, uri);

    request.headerFields.emplace("content-type", contentType);

    request.body = body;

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestSetParameter(
    const std::string& uri,
    const std::string& contentType,
    const std::string& body) noexcept
{
    Request& request =
        *createRequest(Method::SET_PARAMETER, uri);

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

bool Session::handleResponse(std::unique_ptr<Response>& responsePtr) noexcept
{
    auto it = _sentRequests.find(responsePtr->cseq);
    if(it == _sentRequests.end())
        return false;

    const Request& request = it->second;
    const bool success = handleResponse(request, responsePtr);

    _sentRequests.erase(it);

    return success;
}

bool Session::handleResponse(
    const Request& request,
    std::unique_ptr<Response>& responsePtr) noexcept
{
    switch(request.method) {
    case Method::SETUP:
        return onSetupResponse(request, *responsePtr);
    case Method::GET_PARAMETER:
        return onGetParameterResponse(request, *responsePtr);
    case Method::SET_PARAMETER:
        return onSetParameterResponse(request, *responsePtr);
    default:
        return false;
    }
}

bool Session::onSetupResponse(
    const Request& request,
    const Response& response) noexcept
{
    if(StatusCode::OK == response.statusCode)
        return true;

    return false;
}

bool Session::onGetParameterResponse(
    const Request& request,
    const Response& response) noexcept
{
    if(StatusCode::OK == response.statusCode)
        return true;

    return false;
}

bool Session::onSetParameterResponse(
    const Request& request,
    const Response& response) noexcept
{
    if(StatusCode::OK == response.statusCode)
        return true;

    return false;
}

}
