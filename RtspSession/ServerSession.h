#pragma once

#include <memory>
#include <functional>
#include <optional>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"

#include "Session.h"


namespace rtsp {

struct ServerSession : public Session
{
    bool handleRequest(std::unique_ptr<Request>&) noexcept override;

    virtual bool onConnected(const std::optional<std::string>& authCookie) noexcept { return true; }

protected:
    using Session::Session;

    virtual bool onOptionsRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onListRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onDescribeRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onPlayRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onRecordRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onSubscribeRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
    virtual bool onTeardownRequest(std::unique_ptr<Request>&) noexcept
        { return false; }
};

}
