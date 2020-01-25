#pragma once

#include <string>
#include <map>

#include <spdlog/common.h>


typedef std::map<const std::string, const std::string> AuthTokens;

struct InverseProxyServerConfig
{
    spdlog::level::level_enum logLevel = spdlog::level::info;

    std::string serverName;
    std::string certificate;
    std::string key;

    unsigned short frontPort;
    unsigned short secureFrontPort;

    unsigned short backPort;
    unsigned short secureBackPort;

    std::string stunServer;

    std::string turnServer;
    std::string turnUsername;
    std::string turnCredential;
    std::string turnStaticAuthSecret;
    unsigned turnPasswordTTL = 24 * 60 * 60;

    std::string turnsServer;
    std::string turnsUsername;
    std::string turnsCredential;
    std::string turnsStaticAuthSecret;
    unsigned turnsPasswordTTL = 24 * 60 * 60;

    AuthTokens backAuthTokens;
};
