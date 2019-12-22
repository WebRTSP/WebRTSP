#pragma once

#include "RtspSession/ClientSession.h"

#include "ForwardContext.h"


class BackSession : public rtsp::ClientSession
{
public:
    BackSession(
        ForwardContext*,
        const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
        const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept;
    ~BackSession();

private:
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

    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handleSetParameterRequest(std::unique_ptr<rtsp::Request>&) noexcept override;
    bool handleSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
