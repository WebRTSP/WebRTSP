#pragma once

#include "RtspSession/ClientSession.h"


class ClientSession : public rtsp::ClientSession
{
public:
    using rtsp::ClientSession::ClientSession;

    void onConnected() override;

private:
    bool onOptionsResponse(
        const rtsp::Request&, const rtsp::Response&) override;
    bool onDescribeResponse(
        const rtsp::Request&, const rtsp::Response&) override;
    bool onSetupResponse(
        const rtsp::Request&, const rtsp::Response&) override;
    bool onPlayResponse(
        const rtsp::Request&, const rtsp::Response&) override;
    bool onTeardownResponse(
        const rtsp::Request&, const rtsp::Response&) override;
};
