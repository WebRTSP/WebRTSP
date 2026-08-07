#pragma once

#include "RtStreaming/WebRTCPeer.h"

#include "Session.h"
#include "WebRTCSessionMixin.h"


namespace rtsp {

class ClientRecordSession : public Session, public WebRTCSessionMixin
{
public:
    typedef std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> CreatePeer;

    ClientRecordSession(
        const std::string& targetUri,
        const std::string& recordToken,
        const WebRTCConfigPtr&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ~ClientRecordSession();

    const std::shared_ptr<spdlog::logger>& log() const
        { return _log; }

    bool onConnected() noexcept override;

    bool isStreaming() const noexcept;
    void startRecord(const std::string& sourceUri) noexcept;
    void stopRecord() noexcept;

protected:
    bool onOptionsResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onRecordResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onSetupResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onTeardownResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;

    bool onSetupRequest(std::unique_ptr<rtsp::Request>&&) noexcept override;

private:
    const std::shared_ptr<spdlog::logger> _log;

    struct Private;
    std::unique_ptr<Private> _p;
};

}
