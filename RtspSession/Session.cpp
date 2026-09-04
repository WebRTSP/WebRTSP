#include "Session.h"

#include <cassert>
#include <random>


#define SESSION "[{}]" " "
#define TAG "[rtsp::Session]" " "


namespace {

std::string GenerateSessionLogId()
{
    static thread_local std::default_random_engine random((std::random_device()()));
    return std::to_string(random());
}

}

namespace rtsp {

Session::Session(
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    sessionLogId(GenerateSessionLogId()),
    _sendRequest(sendRequest),
    _sendResponse(sendResponse)
{
}

Session::~Session()
{
    log()->info(SESSION TAG "Session destroyed", sessionLogId);
}

Request* Session::createRequest(
    Method method,
    std::string_view uri) noexcept
{
    for(;;) {
        const auto& pair =
            _sentRequests.try_emplace(
                _nextCSeq,
                method,
                std::string(),
                Protocol::WEBRTSP_0_2,
                _nextCSeq);

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
    std::string_view uri,
    const MediaSessionId& session) noexcept
{
    Request* request = createRequest(method, uri);

    request->session = session;

    return request;
}

Request* Session::attachRequest(const std::unique_ptr<rtsp::Request>& requestPtr) noexcept
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

Request* Session::attachRequest(std::unique_ptr<rtsp::Request>&& requestPtr) noexcept
{
    for(;;) {
        const CSeq cseq = _nextCSeq++;
        const auto& pair =
            _sentRequests.emplace(
                cseq,
                std::move(*requestPtr));

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
    out->session = session;

    return out;
}

Response* Session::prepareOkResponse(
    CSeq cseq,
    Response* out)
{
    return prepareResponse(OK, "OK", cseq, {}, out);
}

Response* Session::prepareOkResponse(
    CSeq cseq,
    const MediaSessionId& session,
    Response* out)
{
    return prepareResponse(OK, "OK", cseq, session, out);
}

Response* Session::prepareBadGatewayResponse(
    CSeq cseq,
    Response* out)
{
    return prepareResponse(BAD_GATEWAY, "Bad Gateway", cseq, {}, out);
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
    std::string_view contentType,
    std::string&& body)
{
    Response response;
    prepareOkResponse(cseq, &response);

    response.contentType = contentType;
    response.body = std::move(body);

    sendResponse(response);
}

void Session::sendOkResponse(
    CSeq cseq,
    const MediaSessionId& session,
    std::string_view contentType,
    std::string&& body)
{
    Response response;
    prepareOkResponse(cseq, session, &response);

    response.contentType = contentType;
    response.body = std::move(body);

    sendResponse(response);
}

void Session::sendBadRequestResponse(CSeq cseq)
{
    Response response;
    prepareResponse(BAD_REQUEST, "Bad Request", cseq, {}, &response);
    sendResponse(response);
}

void Session::sendUnauthorizedResponse(CSeq cseq)
{
    Response response;
    prepareResponse(UNAUTHORIZED, "Unauthorized", cseq, {}, &response);
    sendResponse(response);
}

void Session::sendForbiddenResponse(CSeq cseq)
{
    Response response;
    prepareResponse(FORBIDDEN, "Forbidden", cseq, {}, &response);
    sendResponse(response);
}

void Session::sendNotFoundResponse(CSeq cseq)
{
    Response response;
    prepareResponse(NOT_FOUND, "Not Found", cseq, {}, &response);
    sendResponse(response);
}

void Session::sendNotEnoughBandwidth(CSeq cseq)
{
    Response response;
    prepareResponse(NOT_ENOUGH_BANDWIDTH, "Not Enough Bandwidth", cseq, {}, &response);
    sendResponse(response);
}

void Session::sendSessionNotFoundResponse(CSeq cseq)
{
    Response response;
    prepareResponse(SESSION_NOT_FOUND, "Session Not Found", cseq, {}, &response);
    sendResponse(response);
}

void Session::sendInternalErrorResponse(CSeq cseq)
{
    Response response;
    prepareResponse(INTERNAL_ERROR, "Internal Server Error", cseq, {}, &response);
    sendResponse(response);
}

void Session::sendBadGatewayResponse(CSeq cseq)
{
    Response response;
    prepareBadGatewayResponse(cseq, &response);
    sendResponse(response);
}

void Session::sendServiceUnavailableResponse(CSeq cseq)
{
    Response response;
    prepareResponse(SERVICE_UNAVAILABLE, "Service Unavailable", cseq, {}, &response);
    sendResponse(response);
}

void Session::sendRequest(Request& request) noexcept
{
    _sendRequest(&request);
}

CSeq Session::requestOptions(std::string_view uri) noexcept
{
    assert(!uri.empty());
    if(uri.empty())
        return 0;

    Request& request = *createRequest(Method::OPTIONS, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestList(std::string_view uri) noexcept
{
    Request& request = *createRequest(Method::LIST, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::sendList(
    std::string_view uri,
    const std::string& list,
    const std::optional<std::string>& token) noexcept
{
    Request& request = *createRequest(Method::LIST, uri);

    if(token)
        SetBearerAuthorization(&request, token.value());

    request.contentType = TextParametersContentType;
    request.body = list;

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestDescribe(std::string_view uri) noexcept
{
    Request& request = *createRequest(Method::DESCRIBE, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestSetup(
    std::string_view uri,
    std::string_view contentType,
    const MediaSessionId& session,
    std::string&& body) noexcept
{
    assert(!uri.empty());
    assert(!session.empty());

    Request& request = *createRequest(Method::SETUP, uri);

    request.session = session;

    request.contentType = contentType;
    request.body = std::move(body);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestPlay(
    std::string_view uri,
    const MediaSessionId& session,
    const std::string& sdp) noexcept
{
    Request* request = createRequest(Method::PLAY, uri, session);

    request->contentType = SdpContentType;
    request->body = sdp;

    sendRequest(*request);

    return request->cseq;
}

CSeq Session::requestSubscribe(std::string_view uri) noexcept
{
    Request& request = *createRequest(Method::SUBSCRIBE, uri);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestRecord(
    std::string_view uri,
    const std::string& sdp,
    const std::optional<std::string>& token) noexcept
{
    Request& request = *createRequest(Method::RECORD, uri);

    request.contentType = rtsp::SdpContentType;

    if(token)
        SetBearerAuthorization(&request, token.value());

    request.body = sdp;

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestTeardown(
    std::string_view uri,
    const MediaSessionId& session) noexcept
{
    Request& request = *createRequest(Method::TEARDOWN, uri, session);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestGetParameter(
    std::string_view uri,
    std::string_view contentType,
    std::string&& body,
    const std::optional<std::string>& token) noexcept
{
    Request& request = *createRequest(Method::GET_PARAMETER, uri);

    if(token)
        SetBearerAuthorization(&request, token.value());

    request.contentType = contentType;
    request.body = std::move(body);

    sendRequest(request);

    return request.cseq;
}

CSeq Session::requestSetParameter(
    std::string_view uri,
    std::string_view contentType,
    std::string&& body,
    const std::optional<std::string>& token) noexcept
{
    Request& request = *createRequest(Method::SET_PARAMETER, uri);

    if(token)
        SetBearerAuthorization(&request, token.value());

    request.contentType = contentType;
    request.body = std::move(body);

    sendRequest(request);

    return request.cseq;
}

void Session::sendResponse(Response& response) noexcept
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
            SESSION TAG
            "Failed to find sent request corresponding to response with CSeq = {}",
            sessionLogId,
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
