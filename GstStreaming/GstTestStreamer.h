#pragma once

#include "Types.h"
#include "GstStreamer.h"


class GstTestStreamer : public GstStreamer
{
public:
    GstTestStreamer(
        const std::string& pattern = std::string(),
        GstStreaming::Videocodec videocodec = GstStreaming::Videocodec::h264);

protected:
    PrepareResult prepare() override;

private:
    const std::string _pattern;
    GstStreaming::Videocodec _videocodec;
};
