#pragma once

#include <functional>

#include "RtspSession/WebRTCPeer.h"

#include <CxxPtr/GstPtr.h>


class GstWebRTCPeer : public WebRTCPeer
{
public:
    void setRemoteSdp(const std::string& sdp) noexcept override;
    void addIceCandidate(unsigned mlineIndex, const std::string& candidate) noexcept override;
    bool sdp(std::string* sdp) noexcept override;

protected:
    ~GstWebRTCPeer();

    void setPipeline(GstElementPtr&&) noexcept;
    GstElement* pipeline() const noexcept;

    void setWebRtcBin(GstElementPtr&&) noexcept;
    GstElement* webRtcBin() const noexcept;

    void setIceServers(const std::deque<std::string>& iceServers);

    void setState(GstState) noexcept;
    void pause() noexcept;
    void play() noexcept;
    void stop() noexcept;

    void onNegotiationNeeded();
    void onIceGatheringStateChanged();
    void onOfferCreated(GstPromise*);
    void onSetRemoteDescription(GstPromise*);

    virtual void prepared() = 0;
    virtual void iceCandidate(unsigned mlineIndex, const std::string& candidate) = 0;
    virtual void eos(bool error) = 0;

private:
    gboolean onBusMessage(GstBus*, GstMessage*);
    void onIceCandidate(guint candidate, gchar* arg2);

private:
    GstElementPtr _pipelinePtr;
    GstElementPtr _rtcbinPtr;

    GstBusPtr _busPtr;
    guint _busWatchId = 0;

    gulong iceGatheringStateChangedHandlerId = 0;

    std::string _sdp;
};
