#pragma once

#include <string>
#include <memory>
#include <functional>

#include <glib.h>

#include "Config.h"
#include "RtspSession/Session.h"


struct lws_context;

class WsServer final
{
public:
    struct AgentsDb;
    struct SessionFactory;

    WsServer(
        const WsServerConfig&,
        SessionFactory*,
        AgentsDb* = nullptr) noexcept;
    bool init(GMainLoop*, lws_context* = nullptr) noexcept;
    ~WsServer() noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

struct WsServer::AgentsDb
{
    struct AgentCredentials {
        std::string agentId;
        std::string accessToken;
    };

    virtual std::optional<AgentCredentials>
        registerAgent(const std::string& clientId) noexcept = 0;
    virtual bool authenticateAgent(
        const std::string& clientId,
        const std::string& agentId,
        const std::string& accessToken) noexcept = 0;
};

struct WsServer::SessionFactory
{
    virtual std::unique_ptr<rtsp::Session> createSession(
        std::optional<std::string>&& authCookie,
        const rtsp::Session::SendRequest& sendRequest,
        const rtsp::Session::SendResponse& sendResponse) noexcept = 0;

    virtual std::unique_ptr<rtsp::Session> createAgentSession(
        std::string&& clientId,
        std::string&& agentId,
        const rtsp::Session::SendRequest& sendRequest,
        const rtsp::Session::SendResponse& sendResponse) noexcept { return nullptr; };
};
