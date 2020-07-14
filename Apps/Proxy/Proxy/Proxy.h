#pragma once

#include <string>
#include <map>

#include <spdlog/common.h>

#include "Http/Config.h"

#include "Signalling/Config.h"


struct ProxyConfig : public signalling::Config
{
    spdlog::level::level_enum logLevel = spdlog::level::info;

    std::string stunServer;
};

int ProxyMain(const http::Config&, const ProxyConfig&);
