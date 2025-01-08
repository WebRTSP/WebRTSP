#pragma once

#include <optional>
#include <string>
#include <map>


namespace http {

enum {
    DEFAULT_HTTP_PORT = 5080,
    DEFAULT_HTTPS_PORT = 5443,
};

struct Config
{
    bool bindToLoopbackOnly = true;
    unsigned short port = DEFAULT_HTTP_PORT;

    std::string wwwRoot = "./www";

    std::map<std::string, std::string> passwd;
    std::string realm = "WebRTSP";
    std::string opaque = "WebRTSP";

    std::optional<std::string> apiPrefix;

    std::map<std::string, bool> indexPaths; // path -> if auth required for path
};

}
