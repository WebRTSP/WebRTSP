#pragma once

#include <functional>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"


struct RtspSession
{
    RtspSession(const std::function<void (rtsp::Response*)>&);

    bool handleRequest(const rtsp::Request&);

private:
    bool handleOptionsRequest(const rtsp::Request&);
    bool handleDescribeRequest(const rtsp::Request&);
    bool handleSetupRequest(const rtsp::Request&);
    bool handlePlayRequest(const rtsp::Request&);
    bool handleTeardownRequest(const rtsp::Request&);

private:
    std::function<void (rtsp::Response*)> _responseCallback;
};
