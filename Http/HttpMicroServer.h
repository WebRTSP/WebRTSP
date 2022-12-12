#pragma once

#include <string>
#include <memory>
#include <functional>
#include <chrono>

#include <glib.h>

#include "Config.h"


namespace http {

class MicroServer
{
public:
    struct AuthCookieData {
        std::chrono::steady_clock::time_point expiresAt;
        // FIXME! add allowed IP
    };
    typedef std::function<void (const std::string& token, std::chrono::steady_clock::time_point expiresAt)> OnNewAuthToken;

    MicroServer(
        const Config&,
        const std::string& configJs,
        const OnNewAuthToken&,
        GMainContext* context) noexcept;
    bool init() noexcept;
    ~MicroServer();

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
