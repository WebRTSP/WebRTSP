#pragma once

#include "RtStreaming/WebRTCPeer.h"
#include "RtspSession/ClientSession.h"


class ClientRecordSession : public rtsp::ClientSession
{
public:
    ClientRecordSession(
        const std::string& uri,
        const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri) noexcept>& createPeer,
        const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
        const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept;
    ~ClientRecordSession();

    bool onConnected() noexcept override;

    void startRecord();

protected:
    void setUri(const std::string&);

    bool onSetupResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onRecordResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onTeardownResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;

    bool onSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
