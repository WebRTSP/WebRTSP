#include "GstStreamer.h"

#include <cassert>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/webrtc/rtptransceiver.h>

#include <CxxPtr/GlibPtr.h>

#include "LibGst.h"
#include "Helpers.h"


struct GstStreamer::Private
{
    Private(
        GstStreamer* owner,
        GstStreaming::Videocodec videocodec,
        const std::string& pattern);

    GstStreamer *const owner;

    const GstStreaming::Videocodec videocodec;
    const std::string pattern;

    PreparedCallback prepared;
    IceCandidateCallback iceCandidate;
    EosCallback eos;

    GstElementPtr pipelinePtr;

    GstBusPtr busPtr;
    guint busWatchId = 0;

    GstElementPtr rtcbinPtr;

    gulong iceGatheringStateChangedHandlerId = 0;
    std::string sdp;

    void prepare(const IceServers&);
    gboolean onBusMessage(GstBus*, GstMessage*);
    void onNegotiationNeeded(GstElement* rtcbin);
    void onIceGatheringStateChanged(GstElement* rtcbin);
    void onOfferCreated(GstPromise*);
    void onSetRemoteDescription(GstPromise*);
    void onIceCandidate(
        GstElement* rtcbin,
        guint candidate,
        gchar* arg2);

    void setState(GstState);
};

GstStreamer::Private::Private(
    GstStreamer* owner,
    GstStreaming::Videocodec videocodec,
    const std::string& pattern) :
    owner(owner), videocodec(videocodec), pattern(pattern)
{
}

void GstStreamer::Private::prepare(const IceServers& iceServers)
{
    assert(!pipelinePtr);
    if(pipelinePtr)
        return;

    PrepareResult pr = owner->prepare();
    pipelinePtr.swap(pr.pipelinePtr);
    rtcbinPtr.swap(pr.webrtcbinPtr);

    if(!pipelinePtr || !rtcbinPtr)
        return;

    GstElement* pipeline = pipelinePtr.get();
    GstElement* rtcbin = rtcbinPtr.get();

    auto onBusMessageCallback =
        (gboolean (*) (GstBus*, GstMessage*, gpointer))
        [] (GstBus* bus, GstMessage* message, gpointer userData) -> gboolean
        {
            Private* self = static_cast<Private*>(userData);
            return self->onBusMessage(bus, message);
        };

    busPtr.reset(gst_pipeline_get_bus(GST_PIPELINE(pipeline)));
    GstBus* bus = busPtr.get();
    GSourcePtr busSourcePtr(gst_bus_create_watch(bus));
    busWatchId =
        gst_bus_add_watch(bus, onBusMessageCallback, this);

    GstStreaming::SetIceServers(rtcbin, iceServers);

    auto onNegotiationNeededCallback =
        (void (*) (GstElement*, gpointer))
        [] (GstElement* rtcbin, gpointer userData)
        {
            Private* self = static_cast<Private*>(userData);
            return self->onNegotiationNeeded(rtcbin);
        };
    g_signal_connect(rtcbin, "on-negotiation-needed",
        G_CALLBACK(onNegotiationNeededCallback), this);

    auto onIceCandidateCallback =
        (void (*) (GstElement*, guint, gchar*, gpointer))
        [] (GstElement* rtcbin, guint candidate, gchar* arg2, gpointer userData)
        {
            Private* self = static_cast<Private*>(userData);
            return self->onIceCandidate(rtcbin, candidate, arg2);
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

    setState(GST_STATE_PAUSED);
}

gboolean GstStreamer::Private::onBusMessage(GstBus*, GstMessage* msg)
{
    switch(GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            owner->eos(false);
            break;
        case GST_MESSAGE_ERROR: {
            gchar* debug;
            GError* error;

            gst_message_parse_error(msg, &error, &debug);

            g_free(debug);
            g_error_free(error);

            owner->eos(true);
           break;
        }
        default:
            break;
    }

    return TRUE;
}

void GstStreamer::Private::onNegotiationNeeded(GstElement* rtcbin)
{
    auto onIceGatheringStateChangedCallback =
        (void (*) (GstElement*, GParamSpec* , gpointer))
        [] (GstElement* rtcbin, GParamSpec*, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        return self->onIceGatheringStateChanged(rtcbin);
    };
    iceGatheringStateChangedHandlerId =
        g_signal_connect(rtcbin,
            "notify::ice-gathering-state",
            G_CALLBACK(onIceGatheringStateChangedCallback), this);

    auto onOfferCreatedCallback =
        (void (*) (GstPromise*, gpointer))
        [] (GstPromise* promise, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        return self->onOfferCreated(promise);
    };

    GstPromise* promise =
        gst_promise_new_with_change_func(
            onOfferCreatedCallback,
            this, nullptr);
    g_signal_emit_by_name(
        rtcbin, "create-offer", nullptr, promise);
}

void GstStreamer::Private::onIceGatheringStateChanged(GstElement* rtcbin)
{
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
            iceCandidate(0, "a=end-of-candidates");
        }

        g_signal_handler_disconnect(rtcbin, iceGatheringStateChangedHandlerId);
        iceGatheringStateChangedHandlerId = 0;
    }
}

void GstStreamer::Private::onOfferCreated(GstPromise* promise)
{
    GstPromisePtr promisePtr(promise);

    const GstStructure* reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription* sessionDescription = nullptr;
    gst_structure_get(reply, "offer",
        GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
        &sessionDescription, NULL);
    GstWebRTCSessionDescriptionPtr sessionDescriptionPtr(sessionDescription);

    g_signal_emit_by_name(rtcbinPtr.get(),
        "set-local-description", sessionDescription, NULL);

    GCharPtr sdpPtr(gst_sdp_message_as_text(sessionDescription->sdp));
    sdp = sdpPtr.get();

    prepared();
}

void GstStreamer::Private::onIceCandidate(
    GstElement*,
    guint candidate,
    gchar* arg2)
{
    static std::string prefix("a=");
    iceCandidate(candidate, prefix + arg2);
}

void GstStreamer::Private::setState(GstState state)
{
    GstElement* pipeline = pipelinePtr.get();
    if(!pipeline) {
        if(state != GST_STATE_NULL)
            ;
        return;
    }

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


GstStreamer::GstStreamer(
    const std::string& pattern,
    GstStreaming::Videocodec videocodec) :
    _p(new Private(this, videocodec, pattern))
{
}

GstStreamer::~GstStreamer()
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_NULL);
}

void GstStreamer::prepare(
    const IceServers& iceServers,
    const PreparedCallback& prepared,
    const IceCandidateCallback& iceCandidate,
    const EosCallback& eos) noexcept
{
    _p->prepared = prepared;
    _p->iceCandidate = iceCandidate;
    _p->eos = eos;

    _p->prepare(iceServers);
}

bool GstStreamer::sdp(std::string* sdp) noexcept
{
    if(!sdp)
        return false;

    if(_p->sdp.empty())
        return false;

    *sdp = _p->sdp;

    return true;
}

void GstStreamer::setRemoteSdp(const std::string& sdp) noexcept
{
    GstElement* rtcbin = _p->rtcbinPtr.get();

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
        Private* self = static_cast<Private*>(userData);
        return self->onSetRemoteDescription(promise);
    };

    GstPromise* promise =
        gst_promise_new_with_change_func(
            onSetRemoteDescriptionCallback,
            this->_p.get(), nullptr);

    g_signal_emit_by_name(rtcbin,
        "set-remote-description", sessionDescription, promise);
}

void GstStreamer::Private::onSetRemoteDescription(GstPromise* promise)
{
    GstPromisePtr promisePtr(promise);

    GstElement* rtcbin = rtcbinPtr.get();
    GstWebRTCSignalingState state = GST_WEBRTC_SIGNALING_STATE_CLOSED;
    g_object_get(rtcbin, "signaling-state", &state, nullptr);
    if(state != GST_WEBRTC_SIGNALING_STATE_STABLE)
        owner->eos(true);
}

void GstStreamer::addIceCandidate(
    unsigned mlineIndex,
    const std::string& candidate) noexcept
{
    GstElement* rtcbin = _p->rtcbinPtr.get();

    GstWebRTCPeer::addIceCandidate(rtcbin, mlineIndex, candidate);
}

void GstStreamer::eos(bool /*error*/)
{
    if(_p->eos)
        _p->eos();
}

void GstStreamer::play() noexcept
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_PLAYING);
}

void GstStreamer::stop() noexcept
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_NULL);
}
