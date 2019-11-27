#pragma once

#include <memory>
#include <functional>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "Session.h"


namespace rtsp {

struct ServerSession : public Session
{
    using Session::Session;

    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

protected:
    virtual bool handleOptionsRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    virtual bool handleDescribeRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    virtual bool handleSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    virtual bool handlePlayRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    virtual bool handleTeardownRequest(std::unique_ptr<rtsp::Request>&) noexcept;
};

}
