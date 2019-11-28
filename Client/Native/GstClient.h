#pragma once

#include <memory>
#include <functional>


class GstClient
{
public:
    GstClient();
    ~GstClient();

    typedef std::function<void ()> PreparedCallback;
    typedef std::function<
        void (unsigned mlineIndex, const std::string& candidate)> IceCandidateCallback;
    void prepare(
        const PreparedCallback&,
        const IceCandidateCallback&) noexcept;
    bool sdp(std::string* sdp) noexcept;

    void setRemoteSdp(const std::string& sdp) noexcept;
    void addIceCandidate(unsigned mlineIndex, const std::string& candidate);

    void play() noexcept;

private:
    void eos(bool error);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
