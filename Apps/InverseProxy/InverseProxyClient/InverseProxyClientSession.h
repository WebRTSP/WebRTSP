#pragma once

#include "Signalling/ServerSession.h"


class InverseProxyClientSession : public ServerSession
{
public:
    InverseProxyClientSession(
        const std::string& clientName,
        const std::string& authToken,
        const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
        const std::function<void (const rtsp::Request*)>& sendRequest,
        const std::function<void (const rtsp::Response*)>& sendResponse) noexcept;

    bool onConnected() noexcept override ;

protected:
    bool onGetParameterResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;
    bool onSetParameterResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;

private:
    const std::string _clientName;
    const std::string _authToken;

    rtsp::CSeq _authCSeq = 0;
    rtsp::CSeq _turnServerCSeq = 0;

    std::string _turnServer;
};
