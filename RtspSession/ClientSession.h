#pragma once

#include <functional>
#include <map>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "Session.h"


namespace rtsp {

struct ClientSession : public Session
{
    using Session::Session;
    using Session::handleResponse;

protected:
    bool handleResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;

    CSeq requestOptions(const std::string& uri) noexcept;
    CSeq requestDescribe(const std::string& uri) noexcept;
    CSeq requestPlay(const std::string& uri, const rtsp::SessionId&) noexcept;
    CSeq requestTeardown(const std::string& uri, const rtsp::SessionId&) noexcept;

    virtual bool onOptionsResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }
    virtual bool onDescribeResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }
    virtual bool onSetupResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }
    virtual bool onPlayResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }
    virtual bool onTeardownResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }
};

}
