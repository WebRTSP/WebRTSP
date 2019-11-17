#pragma once

#include "Streamer.h"


namespace streaming {

class GstStreamer : public Streamer
{
public:
    bool sdp(std::string* sdp) noexcept override;
};

}
