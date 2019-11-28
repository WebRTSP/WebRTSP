#pragma once

#include "RtspSession/ServerSession.h"

#include "Streaming/GstStreamer.h"


class ServerSession: public rtsp::ServerSession
{
public:
    ServerSession(
        const std::function<void (const rtsp::Request*)>& sendRequest,
        const std::function<void (const rtsp::Response*)>& sendResponse) noexcept;
    ~ServerSession();

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
