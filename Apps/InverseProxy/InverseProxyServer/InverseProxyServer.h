#pragma once

#include <string>
#include <map>


typedef std::map<const std::string, const std::string> AuthTokens;

struct InverseProxyServerConfig
{
    std::string serverName;
    std::string certificate;
    std::string key;

    unsigned short frontPort;
    unsigned short secureFrontPort;

    unsigned short backPort;
    unsigned short secureBackPort;

    AuthTokens backAuthTokens;
};

int InverseProxyServerMain(const InverseProxyServerConfig&);
