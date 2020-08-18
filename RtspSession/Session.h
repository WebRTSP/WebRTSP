#pragma once

#include <memory>
#include <functional>
#include <map>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "StatusCode.h"


namespace rtsp {

struct Session
{
    virtual ~Session() {}

    virtual bool onConnected() noexcept { return true; }

    virtual bool handleRequest(std::unique_ptr<Request>&) noexcept;

    bool handleResponse(std::unique_ptr<Response>& responsePtr) noexcept;

protected:
    Session(
        const std::function<void (const Request*)>& sendRequest,
        const std::function<void (const Response*)>& sendResponse) noexcept;

    Request* createRequest(
        Method,
        const std::string& uri) noexcept;
    Request* createRequest(
        Method,
        const std::string& uri,
        const std::string& session) noexcept;

    static Response* prepareResponse(
        StatusCode statusCode,
        const std::string::value_type* reasonPhrase,
        CSeq cseq,
        const SessionId& session,
        Response* out);
    static Response* prepareOkResponse(
        CSeq cseq,
        const SessionId& session,
        Response* out);
    static Response* prepareOkResponse(
        CSeq cseq,
        Response* out);
    void sendOkResponse(CSeq, const SessionId&);
    void sendOkResponse(
        CSeq,
        const std::string& contentType,
        const std::string& body);
    void sendOkResponse(
        CSeq,
        const SessionId&,
        const std::string& contentType,
        const std::string& body);

    void sendRequest(const Request&) noexcept;
    void sendResponse(const Response&) noexcept;
    void disconnect() noexcept;

    CSeq requestSetup(
        const std::string& uri,
        const std::string& contentType,
        const SessionId& session,
        const std::string& body) noexcept;
    CSeq requestGetParameter(
        const std::string& uri,
        const std::string& contentType,
        const std::string& body) noexcept;
    CSeq requestSetParameter(
        const std::string& uri,
        const std::string& contentType,
        const std::string& body) noexcept;

    virtual bool onGetParameterRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onSetParameterRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onSetupRequest(std::unique_ptr<Request>&) noexcept
        { return false; }

    virtual bool handleResponse(
        const Request&,
        std::unique_ptr<Response>&) noexcept;

    virtual bool onSetupResponse(
        const Request&,
        const Response&) noexcept;
    virtual bool onGetParameterResponse(
        const Request&,
        const Response&) noexcept;
    virtual bool onSetParameterResponse(
        const Request&,
        const Response&) noexcept;

private:
    const std::function<void (const Request*)> _sendRequest;
    const std::function<void (const Response*)> _sendResponse;

    CSeq _nextCSeq = 1;

    std::map<CSeq, Request> _sentRequests;
};

}
