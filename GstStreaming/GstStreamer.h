#pragma once

#include <functional>

#include "GstWebRTCPeer.h"


class GstStreamer : public GstWebRTCPeer
{
public:
    typedef std::function<void ()> PreparedCallback;
    typedef std::function<
        void (unsigned mlineIndex, const std::string& candidate)> IceCandidateCallback;
    void prepare(
        const IceServers&,
        const PreparedCallback&,
        const IceCandidateCallback&,
        const EosCallback&) noexcept override;

protected:
    struct PrepareResult {
        GstElementPtr pipelinePtr;
        GstElementPtr webRtcBinPtr;
    };
    virtual PrepareResult prepare() = 0;

private:
    void prepared() override;
    void iceCandidate(unsigned mlineIndex, const std::string& candidate) override;
    void eos(bool error) override;

private:
    PreparedCallback _preparedCallback;
    IceCandidateCallback _iceCandidateCallback;
    EosCallback _eosCallback;
};
