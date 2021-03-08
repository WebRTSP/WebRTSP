#include "GstWebRTCPeer.h"

#include <cassert>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/webrtc/rtptransceiver.h>

#include <CxxPtr/GlibPtr.h>

#include "Helpers.h"


GstWebRTCPeer::~GstWebRTCPeer()
{
    setState(GST_STATE_NULL);
}

void GstWebRTCPeer::setState(GstState state) noexcept
{
    if(!_pipelinePtr) {
        if(state != GST_STATE_NULL)
            ;
        return;
    }

    GstElement* pipeline = _pipelinePtr.get();

    switch(gst_element_set_state(pipeline, state)) {
        case GST_STATE_CHANGE_FAILURE:
            break;
        case GST_STATE_CHANGE_SUCCESS:
            break;
        case GST_STATE_CHANGE_ASYNC:
            break;
        case GST_STATE_CHANGE_NO_PREROLL:
            break;
    }
}

void GstWebRTCPeer::pause() noexcept
{
    setState(GST_STATE_PAUSED);
}

void GstWebRTCPeer::play() noexcept
{
    setState(GST_STATE_PLAYING);
}

void GstWebRTCPeer::stop() noexcept
{
    setState(GST_STATE_NULL);
}

void GstWebRTCPeer::prepare(
    const IceServers& iceServers,
    const PreparedCallback& prepared,
    const IceCandidateCallback& iceCandidate,
    const EosCallback& eos) noexcept
{
    assert(!pipeline());
    if(pipeline())
        return;

    _iceServers = iceServers;
    _preparedCallback = prepared;
    _iceCandidateCallback = iceCandidate;
    _eosCallback = eos;

    prepare();

    if(!pipeline())
        onEos(true);
}

gboolean GstWebRTCPeer::onBusMessage(GstBus*, GstMessage* msg)
{
    switch(GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            onEos(false);
            break;
        case GST_MESSAGE_ERROR: {
            gchar* debug;
            GError* error;

            gst_message_parse_error(msg, &error, &debug);

            g_free(debug);
            g_error_free(error);

            onEos(true);
            break;
        }
        default:
            break;
    }

    return TRUE;
}

void GstWebRTCPeer::setPipeline(GstElementPtr&& pipelinePtr) noexcept
{
    assert(pipelinePtr);
    assert(!_pipelinePtr);

    if(!pipelinePtr || _pipelinePtr)
        return;

    _pipelinePtr = std::move(pipelinePtr);
    GstElement* pipeline = this->pipeline();

    auto onBusMessageCallback =
        (gboolean (*) (GstBus*, GstMessage*, gpointer))
        [] (GstBus* bus, GstMessage* message, gpointer userData) -> gboolean
        {
            GstWebRTCPeer* self = static_cast<GstWebRTCPeer*>(userData);
            return self->onBusMessage(bus, message);
        };

    _busPtr.reset(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    GstBus* bus = _busPtr.get();
    GSourcePtr busSourcePtr(gst_bus_create_watch(bus));
    _busWatchId =
        gst_bus_add_watch(bus, onBusMessageCallback, this);
}

GstElement* GstWebRTCPeer::pipeline() const noexcept
{
    return _pipelinePtr.get();
}

void GstWebRTCPeer::setWebRtcBin(GstElementPtr&& rtcbinPtr) noexcept
{
    assert(rtcbinPtr);
    assert(!_rtcbinPtr);

    if(!rtcbinPtr || _rtcbinPtr)
        return;

    _rtcbinPtr = std::move(rtcbinPtr);

    GstElement* rtcbin = webRtcBin();

    auto onNegotiationNeededCallback =
        (void (*) (GstElement*, gpointer))
        [] (GstElement* rtcbin, gpointer userData)
        {
            GstWebRTCPeer* self = static_cast<GstWebRTCPeer*>(userData);
            assert(rtcbin == self->webRtcBin());
            return self->onNegotiationNeeded();
        };
    g_signal_connect(rtcbin, "on-negotiation-needed",
        G_CALLBACK(onNegotiationNeededCallback), this);

    auto onIceCandidateCallback =
        (void (*) (GstElement*, guint, gchar*, gpointer))
        [] (GstElement* rtcbin, guint candidate, gchar* arg2, gpointer userData)
        {
            GstWebRTCPeer* self = static_cast<GstWebRTCPeer*>(userData);
            assert(rtcbin == self->webRtcBin());
            return self->onIceCandidate(candidate, arg2);
        };
    g_signal_connect(rtcbin, "on-ice-candidate",
        G_CALLBACK(onIceCandidateCallback), this);

    GArray* transceivers;
    g_signal_emit_by_name(rtcbin, "get-transceivers", &transceivers);
    for(guint i = 0; i < transceivers->len; ++i) {
        GstWebRTCRTPTransceiver* transceiver = g_array_index(transceivers, GstWebRTCRTPTransceiver*, 0);
        transceiver->direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    }
    g_array_unref(transceivers);
}

GstElement*GstWebRTCPeer:: webRtcBin() const noexcept
{
    return _rtcbinPtr.get();
}

void GstWebRTCPeer::setIceServers()
{
    GstElement* rtcbin = webRtcBin();

    guint vMajor = 0, vMinor = 0;
    gst_plugins_base_version(&vMajor, &vMinor, nullptr, nullptr);

    const bool useAddTurnServer =
        vMajor > 1 || (vMajor == 1 && vMinor >= 16);

    for(const std::string& iceServer: _iceServers) {
        using namespace GstStreaming;
        switch(ParseIceServerType(iceServer)) {
            case IceServerType::Stun:
                g_object_set(
                    rtcbin,
                    "stun-server", iceServer.c_str(),
                    nullptr);
                break;
            case IceServerType::Turn:
            case IceServerType::Turns: {
                if(useAddTurnServer) {
                    gboolean ret;
                    g_signal_emit_by_name(
                        rtcbin,
                        "add-turn-server", iceServer.c_str(),
                        &ret);
                } else {
                    g_object_set(
                        rtcbin,
                        "turn-server", iceServer.c_str(),
                        nullptr);
                }
                break;
            }
            default:
                break;
        }
    }
}

void GstWebRTCPeer::onNegotiationNeeded()
{
    GstElement* rtcbin = webRtcBin();

    auto onIceGatheringStateChangedCallback =
        (void (*) (GstElement*, GParamSpec* , gpointer))
        [] (GstElement* rtcbin, GParamSpec*, gpointer userData)
    {
        GstWebRTCPeer* self = static_cast<GstWebRTCPeer*>(userData);
        assert(rtcbin == self->webRtcBin());
        return self->onIceGatheringStateChanged();
    };
    iceGatheringStateChangedHandlerId =
        g_signal_connect(rtcbin,
            "notify::ice-gathering-state",
            G_CALLBACK(onIceGatheringStateChangedCallback), this);

    auto onOfferCreatedCallback =
        (void (*) (GstPromise*, gpointer))
        [] (GstPromise* promise, gpointer userData)
    {
        GstWebRTCPeer* self = static_cast<GstWebRTCPeer*>(userData);
        return self->onOfferCreated(promise);
    };

    GstPromise* promise =
        gst_promise_new_with_change_func(
            onOfferCreatedCallback,
            this, nullptr);
    g_signal_emit_by_name(
        rtcbin, "create-offer", nullptr, promise);
}

void GstWebRTCPeer::onIceGatheringStateChanged()
{
    GstElement* rtcbin = webRtcBin();

    GstWebRTCICEGatheringState state = GST_WEBRTC_ICE_GATHERING_STATE_NEW;
    g_object_get(rtcbin, "ice-gathering-state", &state, NULL);

    if(GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE == state) {
        // "ice-gathering-state" is broken in GStreamer < 1.17.1
        guint gstMajor = 0, gstMinor = 0, gstNano = 0;
        gst_plugins_base_version(&gstMajor, &gstMinor, &gstNano, 0);
        if((gstMajor == 1 && gstMinor == 17 && gstNano >= 1) ||
           (gstMajor == 1 && gstMinor > 17) ||
            gstMajor > 1)
        {
            onIceCandidate(0, std::string());
        }

        g_signal_handler_disconnect(rtcbin, iceGatheringStateChangedHandlerId);
        iceGatheringStateChangedHandlerId = 0;
    }
}

void GstWebRTCPeer::onOfferCreated(GstPromise* promise)
{
    GstElement* rtcbin = webRtcBin();

    GstPromisePtr promisePtr(promise);

    const GstStructure* reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription* sessionDescription = nullptr;
    gst_structure_get(reply, "offer",
        GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
        &sessionDescription, NULL);
    GstWebRTCSessionDescriptionPtr sessionDescriptionPtr(sessionDescription);

    g_signal_emit_by_name(rtcbin,
        "set-local-description", sessionDescription, NULL);

    GCharPtr sdpPtr(gst_sdp_message_as_text(sessionDescription->sdp));
    _sdp = sdpPtr.get();

    onPrepared();
}

void GstWebRTCPeer::addIceCandidate(
    unsigned mlineIndex,
    const std::string& candidate) noexcept
{
    assert(_rtcbinPtr);

    GstElement* rtcbin = _rtcbinPtr.get();

    if(candidate.empty() || candidate == "a=end-of-candidates") {
        guint gstMajor = 0, gstMinor = 0;
        gst_plugins_base_version(&gstMajor, &gstMinor, 0, 0);
        if((gstMajor == 1 && gstMinor > 18) || gstMajor > 1) {
            //"end-of-candidates" support was added only after GStreamer 1.18
            g_signal_emit_by_name(
                rtcbin, "add-ice-candidate",
                mlineIndex, 0);
        }

        return;
    }

    std::string resolvedCandidate;
    if(!candidate.empty())
        ResolveIceCandidate(candidate, &resolvedCandidate);

    if(!resolvedCandidate.empty()) {
        g_signal_emit_by_name(
            rtcbin, "add-ice-candidate",
            mlineIndex, resolvedCandidate.data());
    } else {
        g_signal_emit_by_name(
            rtcbin, "add-ice-candidate",
            mlineIndex, candidate.data());
    }
}

void GstWebRTCPeer::setRemoteSdp(const std::string& sdp) noexcept
{
    GstElement* rtcbin = _rtcbinPtr.get();

    GstSDPMessage* sdpMessage;
    gst_sdp_message_new(&sdpMessage);
    GstSDPMessagePtr sdpMessagePtr(sdpMessage);
    gst_sdp_message_parse_buffer(
        reinterpret_cast<const guint8*>(sdp.data()),
        sdp.size(),
        sdpMessage);

    GstWebRTCSessionDescriptionPtr sessionDescriptionPtr(
        gst_webrtc_session_description_new(
            GST_WEBRTC_SDP_TYPE_ANSWER,
            sdpMessagePtr.release()));
    GstWebRTCSessionDescription* sessionDescription =
        sessionDescriptionPtr.get();

    auto onSetRemoteDescriptionCallback =
        (void (*) (GstPromise*, gpointer))
        [] (GstPromise* promise, gpointer userData)
    {
        GstWebRTCPeer* self = static_cast<GstWebRTCPeer*>(userData);
        return self->onSetRemoteDescription(promise);
    };

    GstPromise* promise =
        gst_promise_new_with_change_func(
            onSetRemoteDescriptionCallback,
            this, nullptr);

    g_signal_emit_by_name(rtcbin,
        "set-remote-description", sessionDescription, promise);
}

bool GstWebRTCPeer::sdp(std::string* sdp) noexcept
{
    if(!sdp)
        return false;

    if(_sdp.empty())
        return false;

    *sdp = _sdp;

    return true;
}

void GstWebRTCPeer::onSetRemoteDescription(GstPromise* promise)
{
    GstElement* rtcbin = _rtcbinPtr.get();

    GstPromisePtr promisePtr(promise);

    GstWebRTCSignalingState state = GST_WEBRTC_SIGNALING_STATE_CLOSED;
    g_object_get(rtcbin, "signaling-state", &state, nullptr);
    if(state != GST_WEBRTC_SIGNALING_STATE_STABLE)
        onEos(true);
}

void GstWebRTCPeer::onPrepared()
{
    if(_preparedCallback)
        _preparedCallback();
}

void GstWebRTCPeer::onIceCandidate(unsigned mlineIndex, const std::string& candidate)
{
    if(_iceCandidateCallback) {
        if(candidate.empty())
            _iceCandidateCallback(mlineIndex, "a=end-of-candidates");
        else
            _iceCandidateCallback(mlineIndex, "a=" + candidate);
    }
}

void GstWebRTCPeer::onEos(bool /*error*/)
{
    if(_eosCallback)
        _eosCallback();
}
