#include "Helpers.h"

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>


namespace GstStreaming
{

IceServerType ParseIceServerType(const std::string& iceServer)
{
    if(0 == iceServer.compare(0, 5, "turn:"))
        return IceServerType::Turn;
    else if(0 == iceServer.compare(0, 6, "turns:"))
        return IceServerType::Turns;
    else if(0 == iceServer.compare(0, 5, "stun:"))
        return IceServerType::Stun;
    else
        return IceServerType::Unknown;
}

}
