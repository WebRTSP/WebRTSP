#pragma once

#include <string>
#include <memory>
#include <functional>

#include <glib.h>

#include "RtspSession/ClientSession.h"

#include "Config.h"


namespace client {

class WsClient
{
public:
    typedef std::function<
        std::unique_ptr<rtsp::ClientSession> (
            const std::function<void (const rtsp::Request*)>& sendRequest,
            const std::function<void (const rtsp::Response*)>& sendResponse)> CreateSession;

    typedef std::function<void (WsClient&)> Disconnected;

    WsClient(
        const Config&,
        GMainLoop*,
        const CreateSession&,
        const Disconnected&) noexcept;
    bool init() noexcept;
    ~WsClient();

    void connect() noexcept;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
