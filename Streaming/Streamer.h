#pragma once

#include <string>


namespace streaming {

struct Streamer
{
    virtual bool sdp(const std::string& uri, std::string* sdp) noexcept;
};

}
