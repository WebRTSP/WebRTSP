#pragma once

#include <string>
#include <map>


namespace http {

struct Config
{
    bool bindToLoopbackOnly = true;
    unsigned short port = 5080;

    std::string wwwRoot = "./www";

    std::map<std::string, std::string> passwd;
    std::string realm = "WebRTSP";
    std::string opaque = "WebRTSP";

    std::map<std::string, bool> indexPaths; // path -> if auth required for path
};

}
