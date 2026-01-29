#pragma once

#include "RtStreaming/WebRTCPeer.h"
#include "Session.h"


namespace rtsp {

class ClientSession : public Session
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

    enum class FeatureState {
        Disabled,
        Enabled,
        Required,
    };
    virtual FeatureState playSupportState(const std::string& /*uri*/) noexcept
        { return FeatureState::Required; }
    virtual FeatureState subscribeSupportState(const std::string& /*uri*/) noexcept
        { return FeatureState::Disabled; }

    CSeq requestDescribe() noexcept;
    CSeq requestSubscribe() noexcept;

    bool onOptionsResponse(
        const Request&, const Response&) noexcept override;
    bool onDescribeResponse(
        const Request&, const Response&) noexcept override;
    bool onSetupResponse(
        const Request&, const Response&) noexcept override;
    bool onPlayResponse(
        const Request&, const Response&) noexcept override;
    bool onSubscribeResponse(
        const Request&, const Response&) noexcept override;
    bool onTeardownResponse(
        const Request&, const Response&) noexcept override;

    bool onRecordRequest(std::unique_ptr<Request>&&) noexcept override;
    bool onSetupRequest(std::unique_ptr<Request>&&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
