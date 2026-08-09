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
    bool init(lws_context* = nullptr) noexcept;
    struct SessionFactory;

    WsServer(const WsServerConfig&, GMainLoop*, SessionFactory*) noexcept;
    ~WsServer() noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

struct WsServer::SessionFactory
{
    virtual std::unique_ptr<rtsp::StreamSession> createSession(
        const rtsp::Session::SendRequest& sendRequest,
        const rtsp::Session::SendResponse& sendResponse) noexcept = 0;
};
