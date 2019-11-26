#pragma once

#include "RtspSession/ClientSession.h"

#include "GstClient.h"


class ClientSession : public rtsp::ClientSession
{
public:
    ClientSession(const std::function<void (const rtsp::Request*)>&);
    ~ClientSession();

    void onConnected() noexcept override;

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

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
