#pragma once

#include "RtspSession/ClientSession.h"
#include "RtspSession/WebRTCPeer.h"


class ClientSession : public rtsp::ClientSession
{
public:
    ClientSession(
        const std::string& uri,
        const std::function<std::unique_ptr<WebRTCPeer> () noexcept>& createPeer,
        const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
        const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept;
    ClientSession(
        const std::function<std::unique_ptr<WebRTCPeer> () noexcept>& createPeer,
        const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
        const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept;
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
