#pragma once

#include <string>
#include <memory>
#include <functional>

#include <glib.h>

#include "Config.h"
#include "RtspSession/StreamSession.h"


struct lws_context;

class WsServer
{
public:
    typedef std::function<
        std::unique_ptr<rtsp::StreamSession> (
            const rtsp::Session::SendRequest& sendRequest,
            const rtsp::Session::SendResponse& sendResponse)> CreateSession;

    WsServer(const WsServerConfig&, GMainLoop*, const CreateSession&) noexcept;
    bool init(lws_context* = nullptr) noexcept;
    ~WsServer() noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
