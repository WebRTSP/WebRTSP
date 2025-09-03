#pragma once

#include "RtStreaming/WebRTCPeer.h"
#include "RtspSession/Session.h"


class ClientSession : public rtsp::Session
{
public:
    typedef std::function<std::unique_ptr<WebRTCPeer> ()> CreatePeer;
    ClientSession(
        const std::string& uri,
        const WebRTCConfigPtr&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ClientSession(
        const WebRTCConfigPtr&,
        const CreatePeer& createPeer,
        const SendRequest& sendRequest,
        const SendResponse& sendResponse) noexcept;
    ~ClientSession();

    bool onConnected() noexcept override;

protected:
    void setUri(const std::string&);

    bool isSupported(rtsp::Method) const noexcept;

    virtual bool playSupportRequired(const std::string& /*uri*/) noexcept { return true; }
    virtual bool recordSupportRequired(const std::string& /*uri*/) noexcept { return false; }

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
