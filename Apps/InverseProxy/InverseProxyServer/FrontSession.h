#pragma once

#include "RtspSession/WebRTCPeer.h"
#include "RtspSession/ServerSession.h"

#include "Forwarder.h"


class FrontSession: public rtsp::Session
{
public:
    FrontSession(
        Forwarder*,
        const std::function<void (const rtsp::Request*)>& sendRequest,
        const std::function<void (const rtsp::Response*)>& sendResponse) noexcept;
    ~FrontSession();

    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

    bool forward(
        BackSession* source,
        std::unique_ptr<rtsp::Request>& sourceRequestPtr);
    bool forward(
        BackSession* source,
        const rtsp::Request&,
        std::unique_ptr<rtsp::Response>&);

    void cancelRequest(const rtsp::CSeq&);

    using rtsp::Session::disconnect;

protected:
    bool forward(std::unique_ptr<rtsp::Request>&);

    bool onOptionsRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    bool onListRequest(std::unique_ptr<rtsp::Request>&) noexcept;

    bool handleResponse(
        const rtsp::Request&,
        std::unique_ptr<rtsp::Response>&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
