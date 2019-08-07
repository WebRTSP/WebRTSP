#pragma once

#include <string>


namespace signalling {

struct Config
{
    std::string serverName;
    std::string certificate;
    std::string key;

    unsigned short port;
    unsigned short securePort;
};

}
