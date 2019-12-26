#pragma once

#include "RtspSession/Session.h"

#include "ForwardContext.h"


class BackSession : public rtsp::Session
{
public:
    BackSession(
        ForwardContext*,
        const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
        const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept;
    ~BackSession();

    rtsp::CSeq forward(std::unique_ptr<rtsp::Request>&);
    void forward(
        const rtsp::Request&,
        const rtsp::Response&);

protected:
    bool handleResponse(
        const rtsp::Request&,
        const rtsp::Response&) noexcept override;

private:
    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handleSetParameterRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handleSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
