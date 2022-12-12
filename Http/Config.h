#pragma once

#include <string>
#include <map>


namespace http {

struct Config
{
    std::string serverName;
    std::string certificate;
    std::string key;

    bool bindToLoopbackOnly = true;
    unsigned short port = 5080;
    bool secureBindToLoopbackOnly = false;
    unsigned short securePort = 5443;

    std::string wwwRoot = "./www";

    std::map<std::string, std::string> passwd;
    std::string realm = "WebRTSP";
    std::string opaque = "WebRTSP";
};

}
