#pragma once

#include <string>
#include <memory>
#include <functional>

#include <glib.h>

#include "RtspSession/Session.h"

#include "Config.h"


class WsClient final
{
public:
    struct SessionFactory;

    typedef std::function<void (WsClient&)> Disconnected;

    WsClient(
        const WsClientConfig&,
        GMainLoop*,
        SessionFactory*,
        const Disconnected&) noexcept;
    bool init() noexcept;
    ~WsClient() noexcept;

    void connect() noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

struct WsClient::SessionFactory
{
    virtual std::unique_ptr<rtsp::Session> createSession(
        const rtsp::Session::SendRequest& sendRequest,
        const rtsp::Session::SendResponse& sendResponse) noexcept { return nullptr; };
};
