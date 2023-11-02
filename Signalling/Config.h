#pragma once

#include <string>


namespace signalling {

struct Config
{
    bool bindToLoopbackOnly = true;
    unsigned short port = 5554;
};

}
