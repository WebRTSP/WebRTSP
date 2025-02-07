#pragma once

#include <string>
#include <memory>
#include <functional>
#include <chrono>

#include <glib.h>

#include "Config.h"

struct MHD_Response;


namespace http {

enum class Method
{
    GET,
    POST,
    PUT,
    DELETE,
    OPTIONS,
    PATCH,
};

class MicroServer
{
public:
    struct AuthCookieData {
        std::chrono::steady_clock::time_point expiresAt;
        // FIXME! add allowed IP
    };
    typedef std::function<void (
        const std::string& token,
        std::chrono::steady_clock::time_point expiresAt)> OnNewAuthToken;
    typedef std::function<std::pair<unsigned, MHD_Response*> (
        Method method,
        const char* uri,
        const std::string_view& body)> APIRequestHandler;

    MicroServer(
        const Config&,
        const std::string& configJs,
        const OnNewAuthToken&,
        const APIRequestHandler&, // will be called from worker thread
        GMainContext* context) noexcept;
    MicroServer(
        const Config& config,
        const std::string& configJs,
        const OnNewAuthToken& newAuthTokenHandler,
        GMainContext* context) noexcept :
        MicroServer(config, configJs, newAuthTokenHandler, APIRequestHandler(), context) {}
    bool init() noexcept;
    ~MicroServer();

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
