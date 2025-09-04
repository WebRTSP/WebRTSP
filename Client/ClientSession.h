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
    bool isPlaySupported() const noexcept;
    bool isSubscribeSupported() const noexcept;

    enum class FeatureState {
        Disabled,
        Enabled,
        Required,
    };
    virtual FeatureState playSupportState(const std::string& /*uri*/) noexcept
        { return FeatureState::Required; }
    virtual FeatureState subscribeSupportState(const std::string& /*uri*/) noexcept
        { return FeatureState::Disabled; }

    rtsp::CSeq requestDescribe() noexcept;
    rtsp::CSeq requestSubscribe() noexcept;

    bool onOptionsResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onDescribeResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onSetupResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onPlayResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onSubscribeResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;
    bool onTeardownResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept override;

    bool onRecordRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
