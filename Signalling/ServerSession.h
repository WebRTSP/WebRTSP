#pragma once

#include "RtspSession/ServerSession.h"

#include "Streaming/GstStreamer.h"


class ServerSession: public rtsp::ServerSession
{
public:
    using rtsp::ServerSession::ServerSession;

protected:
    bool handleOptionsRequest(const rtsp::Request&) noexcept override;
    bool handleDescribeRequest(const rtsp::Request&) noexcept override;
    bool handleSetupRequest(const rtsp::Request&) noexcept override;
    bool handlePlayRequest(const rtsp::Request&) noexcept override;
    bool handleTeardownRequest(const rtsp::Request&) noexcept override;

private:
};
