#pragma once

#include "Token.h"

namespace rtsp {

enum class Authentication
{
    None,
    Unknown,
    Basic,
    Bearer,
};

const char* AuthenticationName(Authentication) noexcept;
Authentication ParseAuthentication(const Token&) noexcept;

}
