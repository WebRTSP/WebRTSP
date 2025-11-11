#pragma once

#include <optional>

#include <spdlog/spdlog.h>

#include "RtStreaming/WebRTCPeer.h"
#include "RtspSession/Session.h"


class ServerSession: public rtsp::Session
{
public:
    typedef std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> CreatePeer;
    ServerSession(
        const WebRTCConfigPtr&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ServerSession(
        const WebRTCConfigPtr&,
        const CreatePeer& createPeer,
        const CreatePeer& createRecordPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ~ServerSession();

    const std::shared_ptr<spdlog::logger>& log() const
        { return _log; }

    virtual bool onConnected(const std::optional<std::string>& authCookie) noexcept;

    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

    void startRecordToClient(const std::string& uri, const rtsp::MediaSessionId&) noexcept;

protected:
    const std::optional<std::string>& authCookie() const noexcept;

    std::string nextSessionId();

    virtual bool listEnabled(const std::string& /*uri*/) noexcept { return false; }
    virtual bool playEnabled(const std::string& uri) noexcept;
    virtual bool recordEnabled(const std::string& uri) noexcept;
    virtual bool subscribeEnabled(const std::string& uri) noexcept;
    virtual bool authorize(const std::unique_ptr<rtsp::Request>&) noexcept;

    bool onGetParameterRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onOptionsRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onDescribeRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onPlayRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onRecordRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onTeardownRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

    bool onRecordResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept override;

    virtual bool isProxyRequest(const rtsp::Request&) noexcept { return false; }
    virtual bool handleProxyRequest(std::unique_ptr<rtsp::Request>&) noexcept { return false; }

    virtual void teardownMediaSession(const rtsp::MediaSessionId&) noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;

    const std::shared_ptr<spdlog::logger> _log;
};
