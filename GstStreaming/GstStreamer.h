#pragma once

#include <memory>
#include <functional>

#include <CxxPtr/GstPtr.h>

#include "GstWebRTCPeer.h"


class GstStreamer : public GstWebRTCPeer
{
public:
    GstStreamer();
    ~GstStreamer();

    typedef std::function<void ()> PreparedCallback;
    typedef std::function<
        void (unsigned mlineIndex, const std::string& candidate)> IceCandidateCallback;
    void prepare(
        const IceServers&,
        const PreparedCallback&,
        const IceCandidateCallback&,
        const EosCallback&) noexcept override;
    bool sdp(std::string* sdp) noexcept override;

    void setRemoteSdp(const std::string& sdp) noexcept override;
    void addIceCandidate(unsigned mlineIndex, const std::string& candidate) noexcept override;

    void play() noexcept override;
    void stop() noexcept override;

protected:
    struct PrepareResult {
        GstElementPtr pipelinePtr;
        GstElementPtr webrtcbinPtr;
    };
    virtual PrepareResult prepare() = 0;

private:
    void eos(bool error);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
