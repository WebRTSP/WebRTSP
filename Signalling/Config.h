#pragma once

#include <string>


namespace signalling {

enum {
    DEFAULT_WS_PORT = 5554,
    DEFAULT_WSS_PORT = 5555,
};

struct Config
{
    bool bindToLoopbackOnly = true;
    unsigned short port = DEFAULT_WS_PORT;
};

}
