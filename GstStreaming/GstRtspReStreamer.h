#pragma once

#include <memory>
#include <functional>

#include "RtspSession/WebRTCPeer.h"


class GstRtspReStreamer : public WebRTCPeer
{
public:
    GstRtspReStreamer(const std::string& rtspSourceUrl);
    ~GstRtspReStreamer();

    typedef std::function<void ()> PreparedCallback;
    typedef std::function<
        void (unsigned mlineIndex, const std::string& candidate)> IceCandidateCallback;
    void prepare(
        const PreparedCallback&,
        const IceCandidateCallback&) noexcept override;
    bool sdp(std::string* sdp) noexcept override;

    void setRemoteSdp(const std::string& sdp) noexcept override;
    void addIceCandidate(unsigned mlineIndex, const std::string& candidate) noexcept override;

    void play() noexcept override;

private:
    void eos(bool error);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
