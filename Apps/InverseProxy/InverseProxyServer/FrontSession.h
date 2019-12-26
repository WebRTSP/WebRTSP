#pragma once

#include "RtspSession/WebRTCPeer.h"
#include "RtspSession/ServerSession.h"

#include "ForwardContext.h"


class FrontSession: public rtsp::Session
{
public:
    FrontSession(
        ForwardContext*,
        const std::function<void (const rtsp::Request*)>& sendRequest,
        const std::function<void (const rtsp::Response*)>& sendResponse) noexcept;
    ~FrontSession();

    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

    rtsp::CSeq forward(const rtsp::Request&);
    void forward(
        const rtsp::Request&,
        const rtsp::Response&);

protected:
    bool handleResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
