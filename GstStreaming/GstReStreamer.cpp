#include "GstReStreamer.h"

#include <cassert>

#include <gst/gst.h>
#include <gst/webrtc/rtptransceiver.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/GstPtr.h>

#include "LibGst.h"
#include "Helpers.h"


struct GstReStreamer::Private
{
    Private(GstReStreamer* owner, const std::string& sourceUri);

    GstReStreamer *const owner;

    const std::string sourceUri;

    PreparedCallback prepared;
    IceCandidateCallback iceCandidate;
    EosCallback eos;

     GstCapsPtr supportedCapsPtr;

    GstElementPtr pipelinePtr;
    GstElement* decodebin = nullptr;

    GstBusPtr busPtr;
    guint busWatchId = 0;

    IceServers iceServers;
    GstElementPtr rtcbinPtr;

    gulong iceGatheringStateChangedHandlerId = 0;
    std::string sdp;

    void prepare(const IceServers&);
    gboolean onBusMessage(GstBus*, GstMessage*);
    void srcPadAdded(GstElement* decodebin, GstPad*);
    void noMorePads(GstElement* decodebin);

    void onNegotiationNeeded(GstElement* rtcbin);
    void onIceGatheringStateChanged(GstElement* rtcbin);
    void onOfferCreated(GstPromise*);
    void onIceCandidate(
        GstElement* rtcbin,
        guint candidate,
        gchar* arg2);

    void setState(GstState);
};

GstReStreamer::Private::Private(
    GstReStreamer* owner,
    const std::string& sourceUri) :
    owner(owner), sourceUri(sourceUri)
{
}

void GstReStreamer::Private::srcPadAdded(
    GstElement* /*decodebin*/,
    GstPad* pad)
{
    GstElement* pipeline = pipelinePtr.get();

    GstCapsPtr capsPtr(gst_pad_get_current_caps(pad));
    GstCaps* caps = capsPtr.get();

    if(gst_caps_is_always_compatible(caps, supportedCapsPtr.get())) {
        GstElement* out =
            gst_parse_bin_from_description(
                "h264parse config-interval=-1 ! rtph264pay pt=96 ! "
                "capssetter caps=\"application/x-rtp,profile-level-id=(string)42c015\" ! "
                "webrtcbin name=srcrtcbin",
                TRUE, NULL);
        gst_bin_add(GST_BIN(pipeline), out);
        gst_element_sync_state_with_parent(out);

        GstPad *sink = (GstPad*)out->sinkpads->data;

        if(GST_PAD_LINK_OK != gst_pad_link(pad, sink))
            assert(false);
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

void GstReStreamer::Private::noMorePads(GstElement* /*decodebin*/)
{
    setState(GST_STATE_PAUSED);
}

void GstReStreamer::Private::prepare(const IceServers& iceServers)
{
    assert(!pipelinePtr);
    if(pipelinePtr)
        return;

    this->iceServers = iceServers;

    supportedCapsPtr.reset(gst_caps_from_string("video/x-h264"));
    GstCaps* supportedCaps = supportedCapsPtr.get();

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

    GstElementPtr srcPtr(gst_element_factory_make("uridecodebin", nullptr));
    decodebin = srcPtr.get();
    if(!decodebin)
        return;

    g_object_set(decodebin, "caps", supportedCaps, nullptr);

    auto srcPadAddedCallback =
        (void (*)(GstElement*, GstPad*, gpointer))
         [] (GstElement* decodebin, GstPad* pad, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        self->srcPadAdded(decodebin, pad);
    };
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(srcPadAddedCallback), this);

    auto noMorePadsCallback =
        (void (*)(GstElement*,  gpointer))
         [] (GstElement* decodebin, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        self->noMorePads(decodebin);
    };
    g_signal_connect(decodebin, "no-more-pads", G_CALLBACK(noMorePadsCallback), this);

    g_object_set(decodebin,
        "uri", sourceUri.c_str(),
        nullptr);

    gst_bin_add(GST_BIN(pipeline), srcPtr.release());

    setState(GST_STATE_PLAYING);
}

gboolean GstReStreamer::Private::onBusMessage(GstBus* bus, GstMessage* msg)
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

void GstReStreamer::Private::onNegotiationNeeded(GstElement* rtcbin)
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

void GstReStreamer::Private::onIceGatheringStateChanged(GstElement* rtcbin)
{
    GstWebRTCICEGatheringState state = GST_WEBRTC_ICE_GATHERING_STATE_NEW;
    g_object_get(rtcbin, "ice-gathering-state", &state, NULL);

    if(GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE == state) {
        iceCandidate(0, "a=end-of-candidates");

        g_signal_handler_disconnect(rtcbin, iceGatheringStateChangedHandlerId);
        iceGatheringStateChangedHandlerId = 0;
    }
}

void GstReStreamer::Private::onOfferCreated(GstPromise* promise)
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

void GstReStreamer::Private::onIceCandidate(
    GstElement* rtcbin,
    guint candidate,
    gchar* arg2)
{
    static std::string prefix("a=");
    iceCandidate(candidate, prefix + arg2);
}

void GstReStreamer::Private::setState(GstState state)
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


GstReStreamer::GstReStreamer(const std::string& sourceUri) :
    _p(new Private(this, sourceUri))
{
}

GstReStreamer::~GstReStreamer()
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_NULL);
}

void GstReStreamer::prepare(
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

bool GstReStreamer::sdp(std::string* sdp) noexcept
{
    if(!sdp)
        return false;

    if(_p->sdp.empty())
        return false;

    *sdp = _p->sdp;

    return true;
}

void GstReStreamer::setRemoteSdp(const std::string& sdp) noexcept
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

void GstReStreamer::addIceCandidate(
    unsigned mlineIndex,
    const std::string& candidate) noexcept
{
    GstElement* rtcbin = _p->rtcbinPtr.get();

    GstWebRTCPeer::addIceCandidate(rtcbin, mlineIndex, candidate);
}

void GstReStreamer::eos(bool error)
{
    if(_p->eos)
        _p->eos();
}

void GstReStreamer::play() noexcept
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_PLAYING);
}

void GstReStreamer::stop() noexcept
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_NULL);
}
