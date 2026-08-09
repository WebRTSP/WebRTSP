#pragma once

#include <memory>
#include <functional>
#include <map>
#include <deque>

#include <spdlog/spdlog.h>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "Log.h"
#include "StatusCode.h"


namespace rtsp {

struct Session
{
    typedef std::function<void (const Request*)> SendRequest;
    typedef std::function<void (const Response*)> SendResponse;

    virtual ~Session();

    const std::string sessionLogId;
    const std::shared_ptr<spdlog::logger>& log() const noexcept
        { return SessionLog(); }

    virtual bool onConnected() noexcept { return true; }

    virtual bool handleRequest(std::unique_ptr<Request>&&) noexcept;
    bool handleResponse(std::unique_ptr<Response>&&) noexcept;

    rtsp::MediaSessionId nextMediaSession()
        { return std::to_string(_nextMediaSession++); }

    Request* createRequest(
        Method,
        std::string_view uri) noexcept;
    Request* createRequest(
        Method,
        std::string_view uri,
        const MediaSessionId&) noexcept;
    Request* attachRequest(const std::unique_ptr<rtsp::Request>&) noexcept;
    Request* attachRequest(std::unique_ptr<rtsp::Request>&&) noexcept;

    static Response* prepareResponse(
        StatusCode,
        const std::string::value_type* reasonPhrase,
        CSeq,
        const MediaSessionId&,
        Response* out);
    static Response* prepareOkResponse(
        CSeq,
        const MediaSessionId&,
        Response* out);
    static Response* prepareOkResponse(
        CSeq,
        Response* out);
    static Response* prepareBadGatewayResponse(
        CSeq,
        const MediaSessionId&,
        Response* out);

    void sendOkResponse(CSeq);
    void sendOkResponse(CSeq, const MediaSessionId&);
    void sendOkResponse(
        CSeq,
        std::string_view contentType,
        std::string&& body);
    void sendOkResponse(
        CSeq,
        const MediaSessionId&,
        std::string_view contentType,
        std::string&& body);

    void sendBadRequestResponse(CSeq);
    void sendUnauthorizedResponse(CSeq);
    void sendForbiddenResponse(CSeq);
    void sendNotFoundResponse(CSeq);
    void sendSessionNotFoundResponse(CSeq, const MediaSessionId&);
    void sendInternalErrorResponse(CSeq);
    void sendBadGatewayResponse(CSeq, const MediaSessionId&);
    void sendServiceUnavailableResponse(CSeq);

    void sendRequest(const Request&) noexcept;
    void sendResponse(const Response&) noexcept;

    void disconnect() noexcept;

    CSeq requestOptions(std::string_view uri) noexcept;
    CSeq requestList(std::string_view uri = rtsp::WildcardUri) noexcept;
    CSeq sendList(
        std::string_view uri,
        const std::string& list,
        const std::optional<std::string>& token = {}) noexcept;
    CSeq requestDescribe(std::string_view uri) noexcept;
    CSeq requestSetup(
        std::string_view uri,
        std::string_view contentType,
        const MediaSessionId& session,
        std::string&& body) noexcept;
    CSeq requestPlay(
        std::string_view uri,
        const MediaSessionId& session,
        const std::string& sdp) noexcept;
    CSeq requestSubscribe(std::string_view uri) noexcept;
    CSeq requestRecord(
        std::string_view uri,
        const std::string& sdp,
        const std::optional<std::string>& token = {}) noexcept;
    CSeq requestTeardown(
        std::string_view uri,
        const MediaSessionId&) noexcept;
    CSeq requestGetParameter(
        std::string_view uri,
        std::string_view contentType,
        std::string&& body,
        const std::optional<std::string>& token = {}) noexcept;
    CSeq requestSetParameter(
        std::string_view uri,
        std::string_view contentType,
        std::string&& body,
        const std::optional<std::string>& token = {}) noexcept;

protected:
    Session(const SendRequest&, const SendResponse&) noexcept;

    virtual bool onOptionsRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }
    virtual bool onListRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }
    virtual bool onDescribeRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }
    virtual bool onSetupRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }
    virtual bool onPlayRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }
    virtual bool onSubscribeRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }
    virtual bool onRecordRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }
    virtual bool onTeardownRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }
    virtual bool onGetParameterRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }
    virtual bool onSetParameterRequest(std::unique_ptr<Request>&&) noexcept
        { return false; }

    virtual bool handleResponse(
        const Request&,
        std::unique_ptr<Response>&&) noexcept;

    virtual bool onOptionsResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }
    virtual bool onListResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }
    virtual bool onDescribeResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }
    virtual bool onSetupResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }
    virtual bool onPlayResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }
    virtual bool onSubscribeResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }
    virtual bool onRecordResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }
    virtual bool onTeardownResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }
    virtual bool onGetParameterResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }
    virtual bool onSetParameterResponse(const Request& request, const Response& response) noexcept
        { return StatusCode::OK == response.statusCode; }

    virtual void onEos() noexcept;

private:
    const SendRequest _sendRequest;
    const SendResponse _sendResponse;

    CSeq _nextCSeq = 1;
    unsigned _nextMediaSession = 1;

    std::map<CSeq, Request> _sentRequests;
};

}
