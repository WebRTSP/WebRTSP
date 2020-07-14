#pragma once

#include "RtspSession/WebRTCPeer.h"

typedef struct _GstElement GstElement;


class GstWebRTCPeer : public WebRTCPeer
{
protected:
    void addIceCandidate(
        GstElement* rtcbin,
        unsigned mlineIndex,
        const std::string& candidate) noexcept;
};
