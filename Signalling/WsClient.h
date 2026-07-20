#pragma once

#include <string>
#include <memory>
#include <functional>

#include <glib.h>

#include "RtspSession/Session.h"

#include "Config.h"


class WsClient
{
public:
    typedef std::function<
        std::unique_ptr<rtsp::Session> (
            const rtsp::Session::SendRequest& sendRequest,
            const rtsp::Session::SendResponse& sendResponse)> CreateSession;

    typedef std::function<void (WsClient&)> Disconnected;

    WsClient(
        const WsClientConfig&,
        GMainLoop*,
        const CreateSession&,
        const Disconnected&) noexcept;
    bool init() noexcept;
    ~WsClient();

    void connect() noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
