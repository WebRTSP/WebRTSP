#pragma once

#include <memory>
#include <functional>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"


namespace rtsp {

struct ServerSession
{
    ServerSession(const std::function<void (const rtsp::Response*)>&) noexcept;

    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept;

protected:
    virtual bool handleOptionsRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    virtual bool handleDescribeRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    virtual bool handleSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    virtual bool handlePlayRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    virtual bool handleTeardownRequest(std::unique_ptr<rtsp::Request>&) noexcept;

    void sendResponse(const rtsp::Response*) noexcept;

private:
    std::function<void (const rtsp::Response*)> _responseCallback;
};

}
