#pragma once

#include <functional>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"


namespace rtsp {

struct ServerSession
{
    ServerSession(const std::function<void (const rtsp::Response*)>&) noexcept;

    bool handleRequest(const rtsp::Request&) noexcept;

protected:
    virtual bool handleOptionsRequest(const rtsp::Request&) noexcept;
    virtual bool handleDescribeRequest(const rtsp::Request&) noexcept;
    virtual bool handleSetupRequest(const rtsp::Request&) noexcept;
    virtual bool handlePlayRequest(const rtsp::Request&) noexcept;
    virtual bool handleTeardownRequest(const rtsp::Request&) noexcept;

    void sendResponse(const rtsp::Response&) noexcept;

private:
    std::function<void (const rtsp::Response*)> _responseCallback;
};

}
