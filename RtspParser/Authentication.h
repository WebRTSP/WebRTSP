#pragma once

#include "Token.h"

namespace rtsp {

enum class Authentication
{
    None,
    Unknown,
    Bearer
};

const char* AuthenticationName(Authentication) noexcept;
Authentication ParseAuthentication(const Token&) noexcept;

}
