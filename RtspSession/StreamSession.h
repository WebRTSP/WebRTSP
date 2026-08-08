#pragma once

#include <optional>

#include <spdlog/spdlog.h>

#include "RtStreaming/WebRTCPeer.h"

#include "Session.h"
#include "WebRTCSessionMixin.h"


namespace rtsp {

class StreamSession: public Session, public WebRTCSessionMixin
{
public:
    typedef std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> CreatePeer;
    StreamSession(
        const WebRTCConfigPtr&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    StreamSession(
        const WebRTCConfigPtr&,
        const CreatePeer& createPeer,
        const CreatePeer& createRecordPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ~StreamSession();

    bool onConnected(const std::optional<std::string>& authCookie = {}) noexcept;

    bool handleRequest(std::unique_ptr<Request>&&) noexcept override;

    void startRecordToClient(const std::string& uri, const MediaSessionId&) noexcept;

protected:
    const std::optional<std::string>& authCookie() const noexcept;

    virtual bool listEnabled(const std::string& /*uri*/) noexcept { return false; }
    virtual bool playEnabled(const std::string& /*uri*/) noexcept { return true; };
    virtual bool recordEnabled(const std::string& /*uri*/) noexcept { return false; }
    virtual bool subscribeEnabled(const std::string& /*uri*/) noexcept { return false; };
    virtual bool authorize(const std::unique_ptr<Request>&) noexcept;

    bool onGetParameterRequest(std::unique_ptr<Request>&&) noexcept override;
    bool onOptionsRequest(std::unique_ptr<Request>&&) noexcept override;
    bool onDescribeRequest(std::unique_ptr<Request>&&) noexcept override;
    bool onSetupRequest(std::unique_ptr<Request>&&) noexcept override;
    bool onPlayRequest(std::unique_ptr<Request>&&) noexcept override;
    bool onRecordRequest(std::unique_ptr<Request>&&) noexcept override;
    bool onTeardownRequest(std::unique_ptr<Request>&&) noexcept override;

    bool onRecordResponse(const Request& request, const Response& response) noexcept override;

    virtual bool isProxyRequest(const Request&) noexcept { return false; }
    virtual bool handleProxyRequest(std::unique_ptr<Request>&) noexcept { return false; }

    virtual void teardownMediaSession(const MediaSessionId&) noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
