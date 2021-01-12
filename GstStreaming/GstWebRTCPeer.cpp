#include "GstWebRTCPeer.h"

#include <cassert>

#include <gst/gst.h>


void GstWebRTCPeer::addIceCandidate(
    GstElement* rtcbin,
    unsigned mlineIndex,
    const std::string& candidate) noexcept
{
    assert(rtcbin);

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
