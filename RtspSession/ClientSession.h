#pragma once

#include <functional>
#include <set>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "Session.h"


namespace rtsp {

struct ClientSession : public Session
{
    bool isSupported(Method);

protected:
    using Session::Session;

    virtual bool playSupportRequired(const std::string& /*uri*/) noexcept { return true; }
    virtual bool recordSupportRequired(const std::string& /*uri*/) noexcept { return false; }

    CSeq requestOptions(const std::string& uri) noexcept;
    CSeq requestDescribe(const std::string& uri) noexcept;
    CSeq requestRecord(
        const std::string& uri,
        const std::string& sdp,
        const std::optional<std::string>& token = {}) noexcept;
    CSeq requestPlay(
        const std::string& uri,
        const MediaSessionId& session,
        const std::string& sdp) noexcept;
    CSeq requestTeardown(const std::string& uri, const MediaSessionId&) noexcept;

    virtual bool onOptionsResponse(
        const Request&, const Response&) noexcept;

private:
    std::set<Method> _supportedMethods;
};

}
