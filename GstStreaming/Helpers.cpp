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

void SetIceServers(
    GstElement* rtcbin,
    const std::deque<std::string>& iceServers)
{
    guint vMajor = 0, vMinor = 0;
    gst_plugins_base_version(&vMajor, &vMinor, nullptr, nullptr);

    const bool useAddTurnServer =
        vMajor > 1 || (vMajor == 1 && vMinor >= 16);

    for(const std::string& iceServer: iceServers) {
        using namespace GstStreaming;
        switch(ParseIceServerType(iceServer)) {
            case IceServerType::Stun:
                g_object_set(
                    rtcbin,
                    "stun-server", iceServer.c_str(),
                    nullptr);
                break;
            case IceServerType::Turn:
            case IceServerType::Turns: {
                if(useAddTurnServer) {
                    gboolean ret;
                    g_signal_emit_by_name(
                        rtcbin,
                        "add-turn-server", iceServer.c_str(),
                        &ret);
                } else {
                    g_object_set(
                        rtcbin,
                        "turn-server", iceServer.c_str(),
                        nullptr);
                }
                break;
            }
            default:
                break;
        }
    }
}

}
