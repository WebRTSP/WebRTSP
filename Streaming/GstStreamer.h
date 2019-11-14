#pragma once

#include "Streamer.h"


namespace streaming {

class GstStreamer : public Streamer
{
public:
    bool sdp(const std::string& uri, std::string* sdp) noexcept override;
};

}
