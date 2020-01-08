#pragma once

#include <string>
#include <map>

#include <spdlog/common.h>

#include "Client/Config.h"


struct StreamerConfig
{
    enum class Type {
        Test,
        ReStreamer,
    };

    Type type;
    std::string uri;
};

struct InverseProxyClientConfig
{
    spdlog::level::level_enum logLevel = spdlog::level::info;

    client::Config clientConfig;
    unsigned reconnectTimeout;
    std::string name;
    std::string authToken;

    std::map<std::string, StreamerConfig> streamers;
};

int InverseProxyClientMain(const InverseProxyClientConfig&);
