#pragma once

#include <string>

#include "Client/Config.h"


struct InverseProxyClientConfig
{
    client::Config clientConfig;
    std::string name;
    std::string authToken;
};

int InverseProxyClientMain(const InverseProxyClientConfig&);
