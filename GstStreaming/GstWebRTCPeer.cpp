#include "GstWebRTCPeer.h"

#include <cassert>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>


void GstWebRTCPeer::addIceCandidate(
    GstElement* rtcbin,
    unsigned mlineIndex,
    const std::string& candidate) noexcept
{
    assert(rtcbin);

    if(candidate.empty() || candidate == "a=end-of-candidates") {
        guint gstMajor = 0, gstMinor = 0;
        gst_plugins_base_version(&gstMajor, &gstMinor, 0, 0);
        if((gstMajor == 1 && gstMinor > 18) || gstMajor > 1) {
            //"end-of-candidates" support was added only after GStreamer 1.18
            g_signal_emit_by_name(
                rtcbin, "add-ice-candidate",
                mlineIndex, 0);
        }

        return;
    }

    std::string resolvedCandidate;
    if(!candidate.empty())
        ResolveIceCandidate(candidate, &resolvedCandidate);

    if(!resolvedCandidate.empty()) {
        g_signal_emit_by_name(
            rtcbin, "add-ice-candidate",
            mlineIndex, resolvedCandidate.data());
    } else {
        g_signal_emit_by_name(
            rtcbin, "add-ice-candidate",
            mlineIndex, candidate.data());
    }
}
