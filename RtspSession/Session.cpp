#include "Session.h"

#include <cassert>

#include <glib.h>

#include "Log.h"


namespace {

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
    _sendResponse(sendResponse),
    _log(MakeRtspSessionLogger(sessionLogId))
{
}

Session::~Session()
{
    log()->info("Session destroyed");
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

void Session::sendBadRequestResponse(CSeq cseq)
{
    Response response;
    prepareResponse(BAD_REQUEST, "Bad Request", cseq, std::string(), &response);
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

void Session::sendNotFoundResponse(CSeq cseq)
{
    Response response;
    prepareResponse(NOT_FOUND, "Not Found", cseq, std::string(), &response);
    sendResponse(response);
}

void Session::sendBadGatewayResponse(CSeq cseq, const MediaSessionId& sessionId)
{
    Response response;
    prepareResponse(BAD_GATEWAY, "Bad Gateway", cseq, sessionId, &response);
    sendResponse(response);
}

void Session::sendServiceUnavailableResponse(CSeq cseq)
{
    Response response;
    prepareResponse(SERVICE_UNAVAILABLE, "Service Unavailable", cseq, std::string(), &response);
    sendResponse(response);
}

void Session::sendRequest(const Request& request) noexcept
{
    _sendRequest(&request);
}

CSeq Session::requestOptions(const std::string& uri) noexcept
{
    assert(!uri.empty());
    if(uri.empty())
        return 0;

    Request& request = *createRequest(Method::OPTIONS, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestList(const std::string& uri) noexcept
{
    Request& request = *createRequest(Method::LIST, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::sendList(
    const std::string& uri,
    const std::string& list,
    const std::optional<std::string>& token) noexcept
{
    Request& request = *createRequest(Method::LIST, uri);

    SetContentType(&request, TextParametersContentType);

    if(token)
        SetBearerAuthorization(&request, token.value());

    request.body = list;

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestDescribe(const std::string& uri) noexcept
{
    Request& request = *createRequest(Method::DESCRIBE, uri);

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

    Request& request = *createRequest(Method::SETUP, uri);

    SetRequestSession(&request, session);
    SetContentType(&request, contentType);

    request.body = body;

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestPlay(
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

CSeq Session::requestSubscribe(const std::string& uri) noexcept
{
    Request& request = *createRequest(Method::SUBSCRIBE, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestRecord(
    const std::string& uri,
    const std::string& sdp,
    const std::optional<std::string>& token) noexcept
{
    Request& request = *createRequest(Method::RECORD, uri);

    SetContentType(&request, rtsp::SdpContentType);

    if(token)
        SetBearerAuthorization(&request, token.value());

    request.body.assign(sdp);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestTeardown(
    const std::string& uri,
    const MediaSessionId& session) noexcept
{
    Request& request = *createRequest(Method::TEARDOWN, uri, session);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestGetParameter(
    const std::string& uri,
    const std::string& contentType,
    const std::string& body,
    const std::optional<std::string>& token) noexcept
{
    Request& request = *createRequest(Method::GET_PARAMETER, uri);

    if(!contentType.empty())
        SetContentType(&request, contentType);

    request.body = body;

    if(token)
        SetBearerAuthorization(&request, token.value());

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestSetParameter(
    const std::string& uri,
    const std::string& contentType,
    const std::string& body,
    const std::optional<std::string>& token) noexcept
{
    Request& request = *createRequest(Method::SET_PARAMETER, uri);

    SetContentType(&request, contentType);

    request.body = body;

    if(token)
        SetBearerAuthorization(&request, token.value());

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

bool Session::handleResponse(std::unique_ptr<Response>&& responsePtr) noexcept
{
    auto it = _sentRequests.find(responsePtr->cseq);
    if(it == _sentRequests.end()) {
        log()->error(
            "Failed to find sent request corresponding to response with CSeq = {}",
            responsePtr->cseq);
        return false;
    }

    const Request& request = it->second;
    const bool success = handleResponse(request, std::move(responsePtr));

    _sentRequests.erase(it);

    return success;
}

bool Session::handleRequest(std::unique_ptr<Request>&& requestPtr) noexcept
{
    switch(requestPtr->method) {
    case Method::NONE:
        break;
    case Method::OPTIONS:
        return onOptionsRequest(std::move(requestPtr));
    case Method::LIST:
        return onListRequest(std::move(requestPtr));
    case Method::DESCRIBE:
        return onDescribeRequest(std::move(requestPtr));
    case Method::SETUP:
        return onSetupRequest(std::move(requestPtr));
    case Method::PLAY:
        return onPlayRequest(std::move(requestPtr));
    case Method::SUBSCRIBE:
        return onSubscribeRequest(std::move(requestPtr));
    case Method::RECORD:
        return onRecordRequest(std::move(requestPtr));
    case Method::TEARDOWN:
        return onTeardownRequest(std::move(requestPtr));
    case Method::GET_PARAMETER:
        return onGetParameterRequest(std::move(requestPtr));
    case Method::SET_PARAMETER:
        return onSetParameterRequest(std::move(requestPtr));
    }

    return false;
}

bool Session::handleResponse(
    const Request& request,
    std::unique_ptr<Response>&& responsePtr) noexcept
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
