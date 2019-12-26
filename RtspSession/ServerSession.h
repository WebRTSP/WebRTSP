#pragma once

#include <memory>
#include <functional>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "Session.h"


namespace rtsp {

struct ServerSession : public Session
{
    bool handleRequest(std::unique_ptr<Request>&) noexcept override;

protected:
    using Session::Session;

    virtual bool handleOptionsRequest(std::unique_ptr<Request>&) noexcept;
    virtual bool handleDescribeRequest(std::unique_ptr<Request>&) noexcept;
    virtual bool handlePlayRequest(std::unique_ptr<Request>&) noexcept;
    virtual bool handleTeardownRequest(std::unique_ptr<Request>&) noexcept;
};

}
