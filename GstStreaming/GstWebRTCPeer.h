#pragma once

#include <functional>

#include "RtspSession/WebRTCPeer.h"

#include <CxxPtr/GstPtr.h>


class GstWebRTCPeer : public WebRTCPeer
{
public:
    void prepare(
        const IceServers&,
        const PreparedCallback&,
        const IceCandidateCallback&,
        const EosCallback&) noexcept override;

    void setRemoteSdp(const std::string& sdp) noexcept override;
    void addIceCandidate(unsigned mlineIndex, const std::string& candidate) noexcept override;
    bool sdp(std::string* sdp) noexcept override;

protected:
    ~GstWebRTCPeer();

    void setPipeline(GstElementPtr&&) noexcept;
    GstElement* pipeline() const noexcept;

    void setWebRtcBin(GstElementPtr&&) noexcept;
    GstElement* webRtcBin() const noexcept;

    void setIceServers();

    void setState(GstState) noexcept;
    void pause() noexcept;
    void play() noexcept;
    void stop() noexcept;

    void onNegotiationNeeded();
    void onIceGatheringStateChanged();
    void onOfferCreated(GstPromise*);
    void onSetRemoteDescription(GstPromise*);

    virtual void prepare() = 0;

private:
    gboolean onBusMessage(GstBus*, GstMessage*);

    void onPrepared();
    void onIceCandidate(unsigned mlineIndex, const std::string& candidate);
    void onEos(bool error);

private:
    std::deque<std::string> _iceServers;
    PreparedCallback _preparedCallback;
    IceCandidateCallback _iceCandidateCallback;
    EosCallback _eosCallback;

    GstElementPtr _pipelinePtr;
    GstElementPtr _rtcbinPtr;

    GstBusPtr _busPtr;
    guint _busWatchId = 0;

    gulong iceGatheringStateChangedHandlerId = 0;

    std::string _sdp;
};
