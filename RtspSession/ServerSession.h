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

    virtual bool onOptionsRequest(std::unique_ptr<Request>&) noexcept;
    virtual bool onListRequest(std::unique_ptr<Request>&) noexcept;
    virtual bool onDescribeRequest(std::unique_ptr<Request>&) noexcept;
    virtual bool onPlayRequest(std::unique_ptr<Request>&) noexcept;
    virtual bool onTeardownRequest(std::unique_ptr<Request>&) noexcept;
};

}
