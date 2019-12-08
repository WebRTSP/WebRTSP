#include "GstStreamer.h"

#include <gst/gst.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/GstPtr.h>


namespace streaming {

struct LibGst
{
    LibGst() { gst_init(0, 0); }
    ~LibGst() { gst_deinit(); }
};

std::unique_ptr<LibGst> libGst;

struct GstStreamer::Private
{
    GstStreamer *const owner;

    PreparedCallback prepared;
    IceCandidateCallback iceCandidate;

    GstElementPtr pipelinePtr;

    GstBusPtr busPtr;
    guint busWatchId = 0;

    GstElementPtr rtcbinPtr;

    gulong iceGatheringStateChangedHandlerId = 0;
    std::string sdp;

    void prepare();
    gboolean onBusMessage(GstBus*, GstMessage*);
    void onNegotiationNeeded(GstElement* rtcbin);
    void onIceGatheringStateChanged(GstElement* rtcbin);
    void onOfferCreated(GstPromise*);
    void onIceCandidate(
        GstElement* rtcbin,
        guint candidate,
        gchar* arg2);

    void setState(GstState);
};

void GstStreamer::Private::prepare()
{
    const char* pipelineDesc =
#if 1
        "videotestsrc ! "
        "x264enc ! video/x-h264, profile=baseline ! rtph264pay pt=96 ! "
        "webrtcbin name=srcrtcbin";
#else
        "videotestsrc pattern=ball ! "
        "vp8enc ! rtpvp8pay pt=96 ! "
        "webrtcbin name=srcrtcbin"
#endif
      ;

    GError* parseError = nullptr;
    pipelinePtr.reset(gst_parse_launch(pipelineDesc, &parseError));
    GErrorPtr parseErrorPtr(parseError);
    if(parseError) {
        return;
    }
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

    rtcbinPtr.reset(
        gst_bin_get_by_name(GST_BIN(pipeline), "srcrtcbin"));
    GstElement* rtcbin = rtcbinPtr.get();

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

    auto onPadAddedCallback =
        (void (*) (GstElement* webrtc, GstPad* pad, gpointer* userData))
        [] (GstElement* webrtc, GstPad* pad, gpointer* userData)
        {
            GstElement* pipeline = reinterpret_cast<GstElement*>(userData);

            if(GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
                return;

            GstElement* out = gst_parse_bin_from_description("rtpvp8depay ! vp8dec ! "
                "videoconvert ! queue ! xvimagesink", TRUE, NULL);
            gst_bin_add(GST_BIN(pipeline), out);
            gst_element_sync_state_with_parent(out);

            GstPad* sink = (GstPad*)out->sinkpads->data;

            gst_pad_link(pad, sink);
        };
    g_signal_connect(rtcbin, "pad-added",
        G_CALLBACK(onPadAddedCallback), pipeline);

    setState(GST_STATE_PAUSED);
}

gboolean GstStreamer::Private::onBusMessage(GstBus* bus, GstMessage* msg)
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

void GstStreamer::Private::onIceGatheringStateChanged(GstElement* rtcbin)
{
    GstWebRTCICEGatheringState state = GST_WEBRTC_ICE_GATHERING_STATE_NEW;
    g_object_get(rtcbin, "ice-gathering-state", &state, NULL);

    if(GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE == state) {
        iceCandidate(0, "a=end-of-candidates");

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
    GstElement* rtcbin,
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


GstStreamer::GstStreamer() :
    _p(new Private{ .owner = this })
{
    if(!libGst)
        libGst = std::make_unique<LibGst>();
}

GstStreamer::~GstStreamer()
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_NULL);
}

void GstStreamer::prepare(
    const PreparedCallback& prepared,
    const IceCandidateCallback& iceCandidate) noexcept
{
    _p->prepared = prepared;
    _p->iceCandidate = iceCandidate;

    _p->prepare();
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
    gst_sdp_message_new_from_text(
        sdp.data(),
        &sdpMessage);
    GstSDPMessagePtr sdpMessagePtr(sdpMessage);

    GstWebRTCSessionDescriptionPtr sessionDescriptionPtr(
        gst_webrtc_session_description_new(
            GST_WEBRTC_SDP_TYPE_ANSWER,
            sdpMessagePtr.release()));
    GstWebRTCSessionDescription* sessionDescription =
        sessionDescriptionPtr.get();

    g_signal_emit_by_name(rtcbin,
        "set-remote-description", sessionDescription, NULL);
}

void GstStreamer::addIceCandidate(
    unsigned mlineIndex,
    const std::string& candidate)
{
    GstElement* rtcbin = _p->rtcbinPtr.get();

    g_signal_emit_by_name(rtcbin, "add-ice-candidate", mlineIndex, candidate.data());
}

void GstStreamer::eos(bool error)
{
}

void GstStreamer::play() noexcept
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_PLAYING);
}

}
