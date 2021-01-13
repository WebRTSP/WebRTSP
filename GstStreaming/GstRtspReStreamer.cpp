#include "GstRtspReStreamer.h"

#include <cassert>

#include <gst/gst.h>
#include <gst/webrtc/rtptransceiver.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/GstPtr.h>

#include "LibGst.h"
#include "Helpers.h"


struct GstRtspReStreamer::Private
{
    Private(GstRtspReStreamer* owner, const std::string& rtspSourceUrl);

    GstRtspReStreamer *const owner;

    const std::string rtspSourceUrl;

    PreparedCallback prepared;
    IceCandidateCallback iceCandidate;
    EosCallback eos;

    GstElementPtr pipelinePtr;
    GstElement* rtspsrc = nullptr;

    GstBusPtr busPtr;
    guint busWatchId = 0;

    IceServers iceServers;
    GstElementPtr rtcbinPtr;

    gulong iceGatheringStateChangedHandlerId = 0;
    std::string sdp;

    void prepare(const IceServers&);
    gboolean onBusMessage(GstBus*, GstMessage*);
    void rtspSrcPadAdded(GstElement* rtspsrc, GstPad*);
    void rtspNoMorePads(GstElement* rtspsrc);

    void onNegotiationNeeded(GstElement* rtcbin);
    void onIceGatheringStateChanged(GstElement* rtcbin);
    void onOfferCreated(GstPromise*);
    void onIceCandidate(
        GstElement* rtcbin,
        guint candidate,
        gchar* arg2);

    void setState(GstState);
};

GstRtspReStreamer::Private::Private(
    GstRtspReStreamer* owner,
    const std::string& rtspSourceUrl) :
    owner(owner), rtspSourceUrl(rtspSourceUrl)
{
}

void GstRtspReStreamer::Private::rtspSrcPadAdded(
    GstElement* /*rtspsrc*/,
    GstPad* pad)
{
    GstElement* pipeline = pipelinePtr.get();

    GstCapsPtr capsPtr(gst_pad_get_current_caps(pad));
    GstCaps* caps = capsPtr.get();
    GCharPtr capsStrPtr(gst_caps_to_string(caps));

    GstStructure* structure = gst_caps_get_structure(caps, 0);

    const gchar* media =
        gst_structure_get_string(structure, "media");

    if(0 == g_strcmp0(media, "video")) {
        GstElement* out =
            gst_parse_bin_from_description(
                "rtph264depay ! h264parse config-interval=-1 ! rtph264pay pt=96 ! "
                "capssetter caps=\"application/x-rtp,profile-level-id=(string)42c015\" ! "
                "webrtcbin name=srcrtcbin",
                TRUE, NULL);
        gst_bin_add(GST_BIN(pipeline), out);
        gst_element_sync_state_with_parent(out);

        GstPad *sink = (GstPad*)out->sinkpads->data;

        gst_pad_link(pad, sink);
    } else
        return;

    rtcbinPtr.reset(
        gst_bin_get_by_name(GST_BIN(pipeline), "srcrtcbin"));
    GstElement* rtcbin = rtcbinPtr.get();

    GstStreaming::SetIceServers(rtcbin, iceServers);
    iceServers.clear();

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
}

void GstRtspReStreamer::Private::rtspNoMorePads(GstElement* /*rtspsrc*/)
{
    setState(GST_STATE_PAUSED);
}

void GstRtspReStreamer::Private::prepare(const IceServers&)
{
    assert(!pipelinePtr);
    if(pipelinePtr)
        return;

    this->iceServers = iceServers;

    pipelinePtr.reset(gst_pipeline_new(nullptr));
    GstElement* pipeline = pipelinePtr.get();

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

    GstElementPtr rtspsrcPtr(gst_element_factory_make("rtspsrc", nullptr));
    rtspsrc = rtspsrcPtr.get();
    if(!rtspsrc)
        return;

    auto rtspSrcPadAddedCallback =
        (void (*)(GstElement*, GstPad*, gpointer))
         [] (GstElement* rtspsrc, GstPad* pad, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        self->rtspSrcPadAdded(rtspsrc, pad);
    };
    g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(rtspSrcPadAddedCallback), this);

    auto rtspNoMorePadsCallback =
        (void (*)(GstElement*,  gpointer))
         [] (GstElement* rtspsrc, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        self->rtspNoMorePads(rtspsrc);
    };
    g_signal_connect(rtspsrc, "no-more-pads", G_CALLBACK(rtspNoMorePadsCallback), this);

    g_object_set(rtspsrc,
        "location", rtspSourceUrl.c_str(),
        nullptr);

    gst_bin_add(GST_BIN(pipeline), rtspsrcPtr.release());

    setState(GST_STATE_PLAYING);
}

gboolean GstRtspReStreamer::Private::onBusMessage(GstBus* bus, GstMessage* msg)
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

void GstRtspReStreamer::Private::onNegotiationNeeded(GstElement* rtcbin)
{
    auto onIceGatheringStateChangedCallback =
        (void (*) (GstElement*, GParamSpec* , gpointer))
        [] (GstElement* rtcbin, GParamSpec* paramSpec, gpointer userData)
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

    GstPromise *promise =
        gst_promise_new_with_change_func(
            onOfferCreatedCallback,
            this, nullptr);
    g_signal_emit_by_name(
        rtcbin, "create-offer", nullptr, promise);
}

void GstRtspReStreamer::Private::onIceGatheringStateChanged(GstElement* rtcbin)
{
    GstWebRTCICEGatheringState state = GST_WEBRTC_ICE_GATHERING_STATE_NEW;
    g_object_get(rtcbin, "ice-gathering-state", &state, NULL);

    if(GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE == state) {
        iceCandidate(0, "a=end-of-candidates");

        g_signal_handler_disconnect(rtcbin, iceGatheringStateChangedHandlerId);
        iceGatheringStateChangedHandlerId = 0;
    }
}

void GstRtspReStreamer::Private::onOfferCreated(GstPromise* promise)
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

void GstRtspReStreamer::Private::onIceCandidate(
    GstElement* rtcbin,
    guint candidate,
    gchar* arg2)
{
    static std::string prefix("a=");
    iceCandidate(candidate, prefix + arg2);
}

void GstRtspReStreamer::Private::setState(GstState state)
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


GstRtspReStreamer::GstRtspReStreamer(const std::string& rtspSource) :
    _p(new Private(this, rtspSource))
{
}

GstRtspReStreamer::~GstRtspReStreamer()
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_NULL);
}

void GstRtspReStreamer::prepare(
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

bool GstRtspReStreamer::sdp(std::string* sdp) noexcept
{
    if(!sdp)
        return false;

    if(_p->sdp.empty())
        return false;

    *sdp = _p->sdp;

    return true;
}

void GstRtspReStreamer::setRemoteSdp(const std::string& sdp) noexcept
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

    g_signal_emit_by_name(rtcbin,
        "set-remote-description", sessionDescription, NULL);
}

void GstRtspReStreamer::addIceCandidate(
    unsigned mlineIndex,
    const std::string& candidate) noexcept
{
    GstElement* rtcbin = _p->rtcbinPtr.get();

    GstWebRTCPeer::addIceCandidate(rtcbin, mlineIndex, candidate);
}

void GstRtspReStreamer::eos(bool error)
{
    if(_p->eos)
        _p->eos();
}

void GstRtspReStreamer::play() noexcept
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_PLAYING);
}

void GstRtspReStreamer::stop() noexcept
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_NULL);
}
