#pragma once

#include <functional>
#include <set>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "Session.h"


namespace rtsp {

struct ClientSession : public Session
{
    using Session::handleResponse;

    bool isSupported(Method);

protected:
    using Session::Session;

    bool handleResponse(
        const Request&,
        std::unique_ptr<Response>&) noexcept override;

    CSeq requestOptions(const std::string& uri) noexcept;
    CSeq requestList() noexcept;
    CSeq requestDescribe(const std::string& uri) noexcept;
    CSeq requestPlay(const std::string& uri, const SessionId&) noexcept;
    CSeq requestTeardown(const std::string& uri, const SessionId&) noexcept;

    virtual bool onOptionsResponse(
        const Request&, const Response&) noexcept;
    virtual bool onListResponse(
        const Request&, const Response&) noexcept
        { return false; }
    virtual bool onDescribeResponse(
        const Request&, const Response&) noexcept
        { return false; }
    virtual bool onPlayResponse(
        const Request&, const Response&) noexcept
        { return false; }
    virtual bool onTeardownResponse(
        const Request&, const Response&) noexcept
        { return false; }

private:
    std::set<Method> _supportedMethods;
};

}
