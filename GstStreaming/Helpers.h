#pragma once

#include <string>
#include <deque>

#include <gst/gst.h>


namespace GstStreaming
{

enum class IceServerType {
    Unknown,
    Stun,
    Turn,
    Turns,
};

IceServerType ParseIceServerType(const std::string& iceServer);
void SetIceServers(
    GstElement* rtcbin,
    const std::deque<std::string>& iceServers);

}
