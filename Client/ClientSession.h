#pragma once

#include "RtStreaming/WebRTCPeer.h"
#include "RtspSession/ClientSession.h"


class ClientSession : public rtsp::ClientSession
{
public:
    typedef std::function<std::unique_ptr<WebRTCPeer> ()> CreatePeer;
    ClientSession(
        const std::string& uri,
        const IceServers&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ClientSession(
        const IceServers&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ~ClientSession();

    bool onConnected() noexcept override;

protected:
    void setUri(const std::string&);

    rtsp::CSeq requestDescribe() noexcept;

    bool onOptionsResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onDescribeResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onSetupResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onPlayResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onTeardownResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;

    bool onSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
