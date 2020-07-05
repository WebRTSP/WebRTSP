#pragma once

#include <string>


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
};

}
