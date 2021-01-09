#pragma once

#include <string>
#include <memory>

#include <glib.h>

#include "RtspSession/ServerSession.h"

#include "Config.h"


struct lws_context;

namespace http {

class Server
{
public:
    Server(const Config&, unsigned short webRTSPPort, GMainLoop*) noexcept;
    bool init(lws_context* = nullptr) noexcept;
    ~Server();

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
