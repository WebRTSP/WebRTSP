#pragma once

#include <cstddef>

#include "Token.h"


namespace rtsp {

enum class Protocol {
    NONE,
    WEBRTSP_0_1
};

const char* ProtocolName(Protocol) noexcept;
Protocol ParseProtocol(const Token& token) noexcept;

}
