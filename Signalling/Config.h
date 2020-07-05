#pragma once

#include <string>


namespace signalling {

struct Config
{
    std::string serverName;
    std::string certificate;
    std::string key;

    bool bindToLoopbackOnly = true;
    unsigned short port = 5554;
    bool secureBindToLoopbackOnly = false;
    unsigned short securePort = 5555;
};

}
