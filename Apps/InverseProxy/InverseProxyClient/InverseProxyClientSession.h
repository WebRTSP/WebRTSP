#pragma once

#include "Signalling/ServerSession.h"


class InverseProxyClientSession : public ServerSession
{
public:
    InverseProxyClientSession(
        const std::string& clientName,
        const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
        const std::function<void (const rtsp::Request*)>& sendRequest,
        const std::function<void (const rtsp::Response*)>& sendResponse) noexcept;

    bool onConnected() noexcept override ;

private:
    const std::string _clientName;
};
