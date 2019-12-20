#pragma once

#include <string>

enum {
    FRONT_SERVER_PORT = 8080,
    BACK_SERVER_PORT = 8081,
};


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
