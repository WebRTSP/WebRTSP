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
    Session(
        const std::function<void (const Request*)>& sendRequest,
        const std::function<void (const Response*)>& sendResponse) noexcept;

    virtual void onConnected() noexcept {}

    virtual bool handleRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    bool handleResponse(const Response&) noexcept;

protected:
    Request* createRequest(
        Method,
        const std::string& uri) noexcept;
    Request* createRequest(
        Method,
        const std::string& uri,
        const std::string& session) noexcept;

    static Response* prepareResponse(
        CSeq cseq,
        StatusCode statusCode,
        const std::string::value_type* reasonPhrase,
        const SessionId& session,
        Response* out);
    static Response* prepareOkResponse(
        CSeq cseq,
        const SessionId& session,
        Response* out);
    void sendOkResponse(rtsp::CSeq, const rtsp::SessionId&);

    void sendRequest(const Request&) noexcept;
    void sendResponse(const Response&) noexcept;
    void disconnect() noexcept;

    virtual bool handleResponse(const Request&, const Response&) noexcept
        { return false; }

private:
    const std::function<void (const Request*)> _sendRequest;
    const std::function<void (const Response*)> _sendResponse;

    CSeq _nextCSeq = 1;

    std::map<CSeq, Request> _sentRequests;
};

}
