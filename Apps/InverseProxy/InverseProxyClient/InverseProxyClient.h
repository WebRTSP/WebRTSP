#pragma once

#include <string>

#include "Client/Config.h"


struct InverseProxyClientConfig
{
    client::Config clientConfig;
    unsigned reconnectTimeout;
    std::string name;
    std::string authToken;
};

int InverseProxyClientMain(const InverseProxyClientConfig&);
