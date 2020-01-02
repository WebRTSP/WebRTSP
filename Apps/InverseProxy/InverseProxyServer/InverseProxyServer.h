#pragma once

#include <string>


struct InverseProxyServerConfig
{
    std::string serverName;
    std::string certificate;
    std::string key;

    unsigned short frontPort;
    unsigned short secureFrontPort;

    unsigned short backPort;
    unsigned short secureBackPort;
};

int InverseProxyServerMain(const InverseProxyServerConfig&);
