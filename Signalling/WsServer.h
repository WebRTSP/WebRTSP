#pragma once

#include <string>
#include <memory>
#include <functional>

#include <glib.h>

#include "Config.h"
#include "RtspSession/StreamSession.h"


struct lws_context;

class WsServer final
{
public:
    struct SessionFactory;

    WsServer(const WsServerConfig&, SessionFactory*) noexcept;
    bool init(GMainLoop*, lws_context* = nullptr) noexcept;
    ~WsServer() noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

struct WsServer::SessionFactory
{
    virtual std::unique_ptr<rtsp::Session> createSession(
        std::optional<std::string>&& authCookie,
        const rtsp::Session::SendRequest& sendRequest,
        const rtsp::Session::SendResponse& sendResponse) noexcept = 0;
};
