#pragma once

#include "Types.h"
#include "GstWebRTCPeer.h"


class GstTestStreamer : public GstWebRTCPeer
{
public:
    GstTestStreamer(
        const std::string& pattern = std::string(),
        GstStreaming::Videocodec videocodec = GstStreaming::Videocodec::h264);

protected:
    void prepare() override;

private:
    const std::string _pattern;
    GstStreaming::Videocodec _videocodec;
};
