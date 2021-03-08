#pragma once

#include <memory>
#include <functional>

#include "GstWebRTCPeer.h"


class GstReStreamer : public GstWebRTCPeer
{
public:
    GstReStreamer(const std::string& sourceUrl);
    ~GstReStreamer();

protected:
    void prepare() noexcept override;

private:
    void srcPadAdded(GstElement* decodebin, GstPad*);
    void noMorePads(GstElement* decodebin);

private:
    const std::string _sourceUrl;

    PreparedCallback _preparedCallback;
    IceCandidateCallback _iceCandidateCallback;
    EosCallback _eosCallback;

    GstCapsPtr _h264CapsPtr;
    GstCapsPtr _vp8CapsPtr;
};
