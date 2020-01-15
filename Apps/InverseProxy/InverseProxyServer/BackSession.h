#pragma once

#include "RtspSession/Session.h"

#include "Forwarder.h"


class BackSession : public rtsp::Session
{
public:
    BackSession(
        Forwarder*,
        const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
        const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept;
    ~BackSession();

    void registerMediaSession(
        FrontSession* target,
        const rtsp::SessionId& targetMediaSession,
        const rtsp::SessionId& mediaSession);
    void unregisterMediaSession(
        FrontSession* target,
        const rtsp::SessionId& targetMediaSession,
        const rtsp::SessionId& mediaSession);

    bool forward(
        FrontSession* source,
        std::unique_ptr<rtsp::Request>& sourceRequestPtr);
    bool forward(const rtsp::Response&);

    void cancelRequest(const rtsp::CSeq&);
    void forceTeardown(const rtsp::SessionId&);

protected:
    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

    bool onGetParameterRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onSetParameterRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool onSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

    bool handleResponse(
        const rtsp::Request&,
        std::unique_ptr<rtsp::Response>&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
