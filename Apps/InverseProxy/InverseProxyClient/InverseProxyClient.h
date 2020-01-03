#pragma once

#include <string>

#include "Client/Config.h"


int InverseProxyClientMain(
    const client::Config&,
    const std::string& clientName,
    const std::string& authToken);
