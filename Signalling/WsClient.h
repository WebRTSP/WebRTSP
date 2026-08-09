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
        std::string&& trustedCAs,
        const WsClientConfig&,
        SessionFactory*,
        const Disconnected&) noexcept;
    WsClient(
        const WsClientConfig& config,
        SessionFactory* sessionFactory,
        const Disconnected& disconnected) noexcept :
        WsClient(std::string(), config, sessionFactory, disconnected) {}
    bool init(GMainLoop*) noexcept;
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
