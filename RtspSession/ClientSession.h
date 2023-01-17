#pragma once

#include <functional>
#include <set>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "Session.h"


namespace rtsp {

struct ClientSession : public Session
{
    virtual bool onConnected() noexcept { return true; }

    bool isSupported(Method);

protected:
    using Session::Session;

    CSeq requestOptions(const std::string& uri) noexcept;
    CSeq requestList() noexcept;
    CSeq requestDescribe(const std::string& uri) noexcept;
    CSeq requestRecord(
        const std::string& uri,
        const std::string& sdp,
        const std::string& token) noexcept;
    CSeq requestPlay(const std::string& uri, const SessionId&) noexcept;
    CSeq requestTeardown(const std::string& uri, const SessionId&) noexcept;

    virtual bool onOptionsResponse(
        const Request&, const Response&) noexcept;

private:
    std::set<Method> _supportedMethods;
};

}
