#pragma once

#include "RtspSession/WebRTCPeer.h"
#include "RtspSession/ServerSession.h"


class ServerSession: public rtsp::ServerSession
{
public:
    ServerSession(
        const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
        const std::function<void (const rtsp::Request*)>& sendRequest,
        const std::function<void (const rtsp::Response*)>& sendResponse) noexcept;
    ~ServerSession();

    void setIceServers(const WebRTCPeer::IceServers&);

private:
    bool onOptionsRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onDescribeRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onPlayRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onTeardownRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
