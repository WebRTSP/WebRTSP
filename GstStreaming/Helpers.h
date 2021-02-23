#pragma once

#include <string>
#include <deque>

#include <gst/gst.h>

#include "Types.h"


namespace GstStreaming
{

IceServerType ParseIceServerType(const std::string& iceServer);
void SetIceServers(
    GstElement* rtcbin,
    const std::deque<std::string>& iceServers);

}
