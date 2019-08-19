#pragma once

#include <cstddef>

#include "Token.h"


namespace rtsp {

enum class Protocol {
    NONE,
    RTSP_1_0
};

const char* ProtocolName(Protocol) noexcept;
Protocol ParseProtocol(const Token& token) noexcept;

}
