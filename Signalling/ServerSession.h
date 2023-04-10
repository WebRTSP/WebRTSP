#pragma once

#include <optional>

#include "RtStreaming/WebRTCPeer.h"
#include "RtspSession/Session.h"


class ServerSession: public rtsp::Session
{
public:
    typedef std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> CreatePeer;
    ServerSession(
        const IceServers&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ServerSession(
        const IceServers&,
        const CreatePeer& createPeer,
        const CreatePeer& createRecordPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ~ServerSession();

    virtual bool onConnected(const std::optional<std::string>& authCookie) noexcept;

    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

    void startRecordToClient(const std::string& uri, const rtsp::SessionId&) noexcept;

protected:
    std::string nextSessionId();

    virtual bool listEnabled() noexcept { return false; }
    virtual bool recordEnabled(const std::string& uri) noexcept;
    virtual bool subscribeEnabled(const std::string& uri) noexcept;
    virtual bool authorize(
        const std::unique_ptr<rtsp::Request>&,
        const std::optional<std::string>& authCookie) noexcept;

    bool onOptionsRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onDescribeRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onPlayRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onRecordRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onTeardownRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

    bool onRecordResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
