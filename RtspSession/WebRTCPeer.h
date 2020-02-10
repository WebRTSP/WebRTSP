#pragma once

#include <string>
#include <functional>
#include <deque>


struct WebRTCPeer
{
    virtual ~WebRTCPeer() {}

    typedef std::deque<std::string> IceServers;
    typedef std::function<void ()> PreparedCallback;
    typedef std::function<
        void (unsigned mlineIndex, const std::string& candidate)> IceCandidateCallback;
    typedef std::function<void ()> EosCallback;
    virtual void prepare(
        const IceServers&,
        const PreparedCallback&,
        const IceCandidateCallback&,
        const EosCallback&) noexcept = 0;

    virtual bool sdp(std::string* sdp) noexcept = 0;

    virtual void setRemoteSdp(const std::string& sdp) noexcept = 0;
    virtual void addIceCandidate(
        unsigned mlineIndex,
        const std::string& candidate) noexcept = 0;

    virtual void play() noexcept = 0;
    virtual void stop() noexcept = 0;
};
