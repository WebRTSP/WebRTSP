#pragma once

#include <functional>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"


namespace rtsp {

struct ServerSession
{
    ServerSession(const std::function<void (const rtsp::Response*)>&);

    bool handleRequest(const rtsp::Request&);

protected:
    virtual bool handleOptionsRequest(const rtsp::Request&);
    virtual bool handleDescribeRequest(const rtsp::Request&);
    virtual bool handleSetupRequest(const rtsp::Request&);
    virtual bool handlePlayRequest(const rtsp::Request&);
    virtual bool handleTeardownRequest(const rtsp::Request&);

    void sendResponse(const rtsp::Response&);

private:
    std::function<void (const rtsp::Response*)> _responseCallback;
};

}
