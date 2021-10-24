#pragma once

#include <cstddef>

#include "Token.h"


namespace rtsp {

enum class Protocol {
    NONE,
    WEBRTSP_0_2,
};

const char* ProtocolName(Protocol) noexcept;
Protocol ParseProtocol(const Token& token) noexcept;

}
