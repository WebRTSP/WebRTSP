#pragma once

#include "RtspSession/WebRTCPeer.h"
#include "RtspSession/ServerSession.h"

#include "ForwardContext.h"


class FrontSession: public rtsp::ServerSession
{
public:
    FrontSession(
        ForwardContext*,
        const std::function<void (const rtsp::Request*)>& sendRequest,
        const std::function<void (const rtsp::Response*)>& sendResponse) noexcept;
    ~FrontSession();

private:
    bool handleOptionsRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handleDescribeRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handleSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handlePlayRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handleTeardownRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
