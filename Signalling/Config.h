#pragma once

#include <cstdint>
#include <string>


namespace signalling {

enum: uint16_t {
    DEFAULT_WS_PORT = 5554,
    DEFAULT_WSS_PORT = 5555,
};

struct Config
{
    bool bindToLoopbackOnly = true;
    unsigned short port = DEFAULT_WS_PORT;
};

}
