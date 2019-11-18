#pragma once

#include <string>


namespace streaming {

struct Streamer
{
    virtual ~Streamer();

    virtual bool sdp(std::string* sdp) noexcept;
};

}
