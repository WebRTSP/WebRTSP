#pragma once

#include <string>
#include <map>

#include <spdlog/common.h>

#include "Signalling/Config.h"


struct ProxyConfig : public signalling::Config
{
    spdlog::level::level_enum logLevel = spdlog::level::info;

    std::string stunServer;
};

int ProxyMain(const ProxyConfig&);
