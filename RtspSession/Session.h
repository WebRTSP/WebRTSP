#pragma once

#include <memory>
#include <functional>
#include <map>
#include <deque>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "StatusCode.h"

#include "RtStreaming/WebRTCConfig.h"


namespace rtsp {

struct Session
{
    typedef std::deque<std::string> IceServers;
    typedef std::function<void (const Request*)> SendRequest;
    typedef std::function<void (const Response*)> SendResponse;

    virtual ~Session();

    const WebRTCConfigPtr& webRTCConfig() const { return _webRTCConfig; }

    virtual bool onConnected() noexcept { return true; }

    virtual bool handleRequest(std::unique_ptr<Request>&) noexcept;
    bool handleResponse(std::unique_ptr<Response>& responsePtr) noexcept;

    const std::string sessionLogId;

protected:
    Session(
        const WebRTCConfigPtr&,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;

    Request* createRequest(
        Method,
        const std::string& uri) noexcept;
    Request* createRequest(
        Method,
        const std::string& uri,
        const std::string& session) noexcept;
    Request* attachRequest(
        const std::unique_ptr<rtsp::Request>& requestPtr) noexcept;

    static Response* prepareResponse(
        StatusCode statusCode,
        const std::string::value_type* reasonPhrase,
        CSeq cseq,
        const MediaSessionId& session,
        Response* out);
    static Response* prepareOkResponse(
        CSeq cseq,
        const MediaSessionId& session,
        Response* out);
    static Response* prepareOkResponse(
        CSeq cseq,
        Response* out);
    void sendOkResponse(CSeq);
    void sendOkResponse(CSeq, const MediaSessionId&);
    void sendOkResponse(
        CSeq,
        const std::string& contentType,
        const std::string& body);
    void sendOkResponse(
        CSeq,
        const MediaSessionId&,
        const std::string& contentType,
        const std::string& body);
    void sendUnauthorizedResponse(CSeq);
    void sendForbiddenResponse(CSeq);
    void sendBadRequestResponse(CSeq);

    void sendRequest(const Request&) noexcept;
    void sendResponse(const Response&) noexcept;
    void disconnect() noexcept;

    CSeq requestList(const std::string& uri = "*") noexcept;
    CSeq sendList(
        const std::string& uri,
        const std::string& list,
        const std::optional<std::string>& token = {}) noexcept;
    CSeq requestSetup(
        const std::string& uri,
        const std::string& contentType,
        const MediaSessionId& session,
        const std::string& body) noexcept;
    CSeq requestGetParameter(
        const std::string& uri,
        const std::string& contentType,
        const std::string& body) noexcept;
    CSeq requestSetParameter(
        const std::string& uri,
        const std::string& contentType,
        const std::string& body) noexcept;

    virtual bool onOptionsRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onListRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onDescribeRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onSetupRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onPlayRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onSubscribeRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onRecordRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onTeardownRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onGetParameterRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onSetParameterRequest(std::unique_ptr<Request>&) noexcept
        { return false; }

    virtual bool handleResponse(
        const Request&,
        std::unique_ptr<Response>&) noexcept;

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
    const WebRTCConfigPtr _webRTCConfig;

    const SendRequest _sendRequest;
    const SendResponse _sendResponse;

    CSeq _nextCSeq = 1;

    std::map<CSeq, Request> _sentRequests;
};

}
