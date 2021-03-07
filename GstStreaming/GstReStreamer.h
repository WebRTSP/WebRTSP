#pragma once

#include <memory>
#include <functional>

#include "GstWebRTCPeer.h"


class GstReStreamer : public GstWebRTCPeer
{
public:
    GstReStreamer(const std::string& sourceUrl);
    ~GstReStreamer();

    typedef std::function<void ()> PreparedCallback;
    typedef std::function<
        void (unsigned mlineIndex, const std::string& candidate)> IceCandidateCallback;
    void prepare(
        const IceServers&,
        const PreparedCallback&,
        const IceCandidateCallback&,
        const EosCallback&) noexcept override;

private:
    void srcPadAdded(GstElement* decodebin, GstPad*);
    void noMorePads(GstElement* decodebin);

    void prepared() override;
    void iceCandidate(unsigned mlineIndex, const std::string& candidate) override;
    void eos(bool error) override;

private:
    const std::string _sourceUrl;

    IceServers _iceServers;

    PreparedCallback _preparedCallback;
    IceCandidateCallback _iceCandidateCallback;
    EosCallback _eosCallback;

    GstCapsPtr _h264CapsPtr;
    GstCapsPtr _vp8CapsPtr;
};
