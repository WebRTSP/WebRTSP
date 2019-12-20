#pragma once

#include <string>
#include <memory>
#include <functional>

#include <glib.h>

#include "RtspSession/ServerSession.h"

#include "Config.h"


struct lws_context;

namespace signalling {

class WsServer
{
public:
    typedef std::function<
        std::unique_ptr<rtsp::Session> (
            const std::function<void (const rtsp::Request*)>& sendRequest,
            const std::function<void (const rtsp::Response*)>& sendResponse) noexcept> CreateSession;

    WsServer(const Config&, GMainLoop*, const CreateSession&) noexcept;
    bool init(lws_context* = nullptr) noexcept;
    ~WsServer();

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
