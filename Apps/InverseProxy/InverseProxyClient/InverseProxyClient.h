#pragma once

#include <string>

#include <spdlog/common.h>

#include "Client/Config.h"


struct InverseProxyClientConfig
{
    spdlog::level::level_enum logLevel = spdlog::level::info;

    client::Config clientConfig;
    unsigned reconnectTimeout;
    std::string name;
    std::string authToken;
};

int InverseProxyClientMain(const InverseProxyClientConfig&);
