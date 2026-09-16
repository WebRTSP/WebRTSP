#pragma once

#include <string>
#include <memory>
#include <functional>
#include <chrono>

#include <glib.h>

#include <microhttpd.h>

#include "Config.h"


namespace http
{

enum class Method
{
    GET,
    POST,
    PUT,
    DELETE,
    OPTIONS,
    PATCH,
};

typedef unsigned StatusCode;

inline MHD_Response*
FixResponse(MHD_Response* response)
{
    return response ? response : MHD_create_response_from_buffer_static(0, nullptr);
}

inline std::pair<StatusCode, MHD_Response*>
OK(MHD_Response* response = nullptr)
{
    return { MHD_HTTP_OK, FixResponse(response) };
}

inline std::pair<StatusCode, MHD_Response*>
Created(MHD_Response* response = nullptr)
{
    return { MHD_HTTP_CREATED, FixResponse(response) };
}

inline std::pair<StatusCode, MHD_Response*>
InternalError(MHD_Response* response = nullptr)
{
    return { MHD_HTTP_INTERNAL_SERVER_ERROR, FixResponse(response) };
}

inline std::pair<StatusCode, MHD_Response*>
BadRequest(MHD_Response* response = nullptr)
{
    return { MHD_HTTP_BAD_REQUEST, FixResponse(response) };
}

inline std::pair<StatusCode, MHD_Response*>
NotFound(MHD_Response* response = nullptr)
{
    return { MHD_HTTP_NOT_FOUND, FixResponse(response) };
}

inline std::pair<StatusCode, MHD_Response*>
TooManyRequests(MHD_Response* response = nullptr)
{
    return { MHD_HTTP_TOO_MANY_REQUESTS, FixResponse(response) };
}

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
    typedef std::function<std::pair<StatusCode, MHD_Response*> (
        Method method,
        const char* uri,
        std::string_view body)> APIRequestHandler;

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
