#pragma once

#include "RtspSession/ServerSession.h"


class ServerSession: public rtsp::ServerSession
{
public:
    using rtsp::ServerSession::ServerSession;

protected:
    bool handleOptionsRequest(const rtsp::Request&) override;
    bool handleDescribeRequest(const rtsp::Request&) override;
    bool handleSetupRequest(const rtsp::Request&) override;
    bool handlePlayRequest(const rtsp::Request&) override;
    bool handleTeardownRequest(const rtsp::Request&) override;

private:
};
