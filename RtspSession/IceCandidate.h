#pragma once

#include <string>


namespace rtsp {

struct IceCandidate {
    unsigned mlineIndex;
    std::string candidate;
};

}
