#include "Session.h"

#include <cassert>

#include <glib.h>

#include "Log.h"


namespace {

const auto Log = RtspSessionLog;

std::string GenerateSessionLogId()
{
    g_autoptr(GRand) rand = g_rand_new();
    return std::to_string(g_rand_int(rand));
}

}

namespace rtsp {

Session::Session(
    const WebRTCConfigPtr& webRTCConfig,
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    sessionLogId(GenerateSessionLogId()),
    _webRTCConfig(webRTCConfig),
    _sendRequest(sendRequest),
    _sendResponse(sendResponse)
{
}

Session::~Session()
{
    Log()->info("[{}] Session destroyed", sessionLogId);
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
                    .protocol = Protocol::WEBRTSP_0_2,
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
    const MediaSessionId& session) noexcept
{
    Request* request = createRequest(method, uri);

    SetRequestSession(request, session);

    return request;
}

Request* Session::attachRequest(
    const std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    for(;;) {
        const CSeq cseq = _nextCSeq++;
        const auto& pair =
            _sentRequests.emplace(
                cseq,
                *requestPtr);

        if(pair.second) {
            Request& request = pair.first->second;
            request.cseq = cseq;
            return &request;
        }
    }
}

Response* Session::prepareResponse(
    StatusCode statusCode,
    const std::string::value_type* reasonPhrase,
    CSeq cseq,
    const MediaSessionId& session,
    Response* out)
{
    out->protocol = Protocol::WEBRTSP_0_2;
    out->cseq = cseq;
    out->statusCode = statusCode;
    out->reasonPhrase = reasonPhrase;

    if(!session.empty())
        SetResponseSession(out, session);

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
    const MediaSessionId& session,
    Response* out)
{
    return prepareResponse(OK, "OK", cseq, session, out);
}

void Session::sendOkResponse(CSeq cseq)
{
    Response response;
    sendResponse(*prepareOkResponse(cseq, &response));
}

void Session::sendOkResponse(
    CSeq cseq,
    const MediaSessionId& session)
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

    SetContentType(&response, contentType);

    response.body = body;

    sendResponse(response);
}

void Session::sendOkResponse(
    CSeq cseq,
    const MediaSessionId& session,
    const std::string& contentType,
    const std::string& body)
{
    Response response;
    prepareOkResponse(cseq, session, &response);

    SetContentType(&response, contentType);

    response.body = body;

    sendResponse(response);
}

void Session::sendUnauthorizedResponse(CSeq cseq)
{
    Response response;
    prepareResponse(UNAUTHORIZED, "Unauthorized", cseq, std::string(), &response);
    sendResponse(response);
}

void Session::sendForbiddenResponse(CSeq cseq)
{
    Response response;
    prepareResponse(FORBIDDEN, "Forbidden", cseq, std::string(), &response);
    sendResponse(response);
}

void Session::sendBadRequestResponse(CSeq cseq)
{
    Response response;
    prepareResponse(BAD_REQUEST, "Bad Request", cseq, std::string(), &response);
    sendResponse(response);
}

void Session::sendRequest(const Request& request) noexcept
{
    _sendRequest(&request);
}

CSeq Session::requestList(const std::string& uri) noexcept
{
    Request& request =
        *createRequest(Method::LIST, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::sendList(
    const std::string& uri,
    const std::string& list,
    const std::string& token) noexcept
{
    Request& request =
        *createRequest(Method::LIST, uri);

    SetContentType(&request, TextParametersContentType);

    if(!token.empty())
        request.headerFields.emplace("Authorization", "Bearer " + token);

    request.body = list;

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestSetup(
    const std::string& uri,
    const std::string& contentType,
    const MediaSessionId& session,
    const std::string& body) noexcept
{
    assert(!uri.empty());
    assert(!session.empty());

    Request& request =
        *createRequest(Method::SETUP, uri);

    SetRequestSession(&request, session);
    SetContentType(&request, contentType);

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

    SetContentType(&request, contentType);

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

    SetContentType(&request, contentType);

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
    if(it == _sentRequests.end()) {
        Log()->error(
            "[{}] Failed to find sent request corresponding to response with CSeq = {}",
            sessionLogId,
            responsePtr->cseq);
        return false;
    }

    const Request& request = it->second;
    const bool success = handleResponse(request, responsePtr);

    _sentRequests.erase(it);

    return success;
}

bool Session::handleRequest(std::unique_ptr<Request>& requestPtr) noexcept
{
    switch(requestPtr->method) {
    case Method::NONE:
        break;
    case Method::OPTIONS:
        return onOptionsRequest(requestPtr);
    case Method::LIST:
        return onListRequest(requestPtr);
    case Method::DESCRIBE:
        return onDescribeRequest(requestPtr);
    case Method::SETUP:
        return onSetupRequest(requestPtr);
    case Method::PLAY:
        return onPlayRequest(requestPtr);
    case Method::SUBSCRIBE:
        return onSubscribeRequest(requestPtr);
    case Method::RECORD:
        return onRecordRequest(requestPtr);
    case Method::TEARDOWN:
        return onTeardownRequest(requestPtr);
    case Method::GET_PARAMETER:
        return onGetParameterRequest(requestPtr);
    case Method::SET_PARAMETER:
        return onSetParameterRequest(requestPtr);
    }

    return false;
}

bool Session::handleResponse(
    const Request& request,
    std::unique_ptr<Response>& responsePtr) noexcept
{
    switch(request.method) {
    case Method::NONE:
        break;
    case Method::OPTIONS:
        return onOptionsResponse(request, *responsePtr);
    case Method::LIST:
        return onListResponse(request, *responsePtr);
    case Method::DESCRIBE:
        return onDescribeResponse(request, *responsePtr);
    case Method::SETUP:
        return onSetupResponse(request, *responsePtr);
    case Method::PLAY:
        return onPlayResponse(request, *responsePtr);
    case Method::SUBSCRIBE:
        return onSubscribeResponse(request, *responsePtr);
    case Method::RECORD:
        return onRecordResponse(request, *responsePtr);
    case Method::TEARDOWN:
        return onTeardownResponse(request, *responsePtr);
    case Method::GET_PARAMETER:
        return onGetParameterResponse(request, *responsePtr);
    case Method::SET_PARAMETER:
        return onSetParameterResponse(request, *responsePtr);
    }

    return false;
}

void Session::onEos() noexcept
{
    disconnect();
}

}
