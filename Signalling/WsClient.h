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
    // server can register new agent if agentId or accessToken is empty
    void connectAsAgent(
        const std::string& clientId,
        const std::string& agentId = {},
        const std::string& accessToken = {}) noexcept;
    void disconnect() noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

struct WsClient::SessionFactory
{
    virtual std::unique_ptr<rtsp::Session> createSession(
        const rtsp::Session::SendRequest& sendRequest,
        const rtsp::Session::SendResponse& sendResponse) noexcept { return nullptr; };

    virtual std::unique_ptr<rtsp::Session> createAgentSession(
        const std::string& clientId,
        std::string&& agentId,
        std::string&& accessToken,
        const rtsp::Session::SendRequest& sendRequest,
        const rtsp::Session::SendResponse& sendResponse) noexcept { return nullptr; };
};
