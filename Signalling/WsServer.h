#pragma once

#include <string>
#include <memory>
#include <functional>

#include <glib.h>

#include "Config.h"
#include "RtspSession/ServerSession.h"


struct lws_context;

namespace signalling {

class WsServer
{
public:
    typedef std::function<
        std::unique_ptr<rtsp::ServerSession> (
            const rtsp::Session::SendRequest& sendRequest,
            const rtsp::Session::SendResponse& sendResponse)> CreateSession;

    WsServer(const Config&, GMainLoop*, const CreateSession&) noexcept;
    bool init(lws_context* = nullptr) noexcept;
    ~WsServer();

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
