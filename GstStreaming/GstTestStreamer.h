#pragma once

#include <memory>
#include <functional>

#include "Types.h"
#include "GstWebRTCPeer.h"


class GstTestStreamer : public GstWebRTCPeer
{
public:
    GstTestStreamer(
        const std::string& pattern = std::string(),
        GstStreaming::Videocodec videocodec = GstStreaming::Videocodec::h264);
    ~GstTestStreamer();

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

private:
    void eos(bool error);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
