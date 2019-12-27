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

    bool forward(
        FrontSession* source,
        std::unique_ptr<rtsp::Request>& sourceRequestPtr);
    bool forward(const rtsp::Response&);

protected:
    bool handleResponse(
        const rtsp::Request&,
        std::unique_ptr<rtsp::Response>&) noexcept override;

private:
    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handleSetParameterRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handleSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

    bool manageMediaSessions(
        const rtsp::Request&,
        const rtsp::Response&) noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
