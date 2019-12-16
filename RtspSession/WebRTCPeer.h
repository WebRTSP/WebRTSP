#pragma once

#include <string>
#include <functional>


struct WebRTCPeer
{
    virtual ~WebRTCPeer() {}

    typedef std::function<void ()> PreparedCallback;
    typedef std::function<
        void (unsigned mlineIndex, const std::string& candidate)> IceCandidateCallback;
    virtual void prepare(
        const PreparedCallback&,
        const IceCandidateCallback&) noexcept = 0;

    virtual bool sdp(std::string* sdp) noexcept = 0;

    virtual void setRemoteSdp(const std::string& sdp) noexcept = 0;
    virtual void addIceCandidate(
        unsigned mlineIndex,
        const std::string& candidate) noexcept = 0;

    virtual void play() noexcept = 0;
    virtual void stop() noexcept = 0;
};
