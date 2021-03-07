#include "GstClient.h"

#include <cassert>
#include <deque>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/GstPtr.h>

#include "LibGst.h"


struct GstClient::Private
{
    Private(GstClient* owner);

    GstClient *const owner;

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
    void onIceGatheringStateChanged(GstElement* rtcbin);
    void onAnswerCreated(GstPromise*);
    void onIceCandidate(
        GstElement* rtcbin,
        guint candidate,
        gchar* arg2);

    void setState(GstState);
};

GstClient::Private::Private(GstClient* owner) :
    owner(owner)
{
}

void GstClient::Private::prepare(const IceServers&)
{
    assert(!pipelinePtr);
    if(pipelinePtr)
        return;

    pipelinePtr.reset(gst_pipeline_new("Client Pipeline"));
    GstElement* pipeline = pipelinePtr.get();

    rtcbinPtr.reset(
        gst_element_factory_make("webrtcbin", "clientrtcbin"));
    GstElement* rtcbin = rtcbinPtr.get();

    gst_bin_add_many(GST_BIN(pipeline), rtcbin, NULL);
    gst_object_ref(rtcbin);

    GstCapsPtr capsPtr(gst_caps_from_string("application/x-rtp, media=video"));
    GstWebRTCRTPTransceiver* recvonlyTransceiver = nullptr;
    g_signal_emit_by_name(
        rtcbin, "add-transceiver",
        GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, capsPtr.get(),
        &recvonlyTransceiver);
    GstWebRTCRTPTransceiverPtr recvonlyTransceiverPtr(recvonlyTransceiver);

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
        GstPad *sink;

        GstElement* pipeline = reinterpret_cast<GstElement*>(userData);

        if(GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
            return;

        GstElement* out =
            gst_parse_bin_from_description(
#if 1
                "rtph264depay ! avdec_h264 ! "
#else
                "rtpvp8depay ! vp8dec ! "
#endif
                "videoconvert ! queue ! "
                "autovideosink", TRUE, NULL);
        gst_bin_add(GST_BIN(pipeline), out);
        gst_element_sync_state_with_parent(out);

        sink = (GstPad*)out->sinkpads->data;

        gst_pad_link(pad, sink);
    };
    g_signal_connect(rtcbin, "pad-added",
        G_CALLBACK(onPadAddedCallback), pipeline);

    setState(GST_STATE_PAUSED);
}

gboolean GstClient::Private::onBusMessage(GstBus* bus, GstMessage* msg)
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

void GstClient::Private::onIceGatheringStateChanged(GstElement* rtcbin)
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

void GstClient::Private::onAnswerCreated(GstPromise* promise)
{
    GstPromisePtr promisePtr(promise);

    const GstStructure* reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription* sessionDescription = nullptr;
    gst_structure_get(reply, "answer",
        GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
        &sessionDescription, NULL);
    GstWebRTCSessionDescriptionPtr sessionDescriptionPtr(sessionDescription);

    g_signal_emit_by_name(rtcbinPtr.get(),
        "set-local-description", sessionDescription, NULL);

    GCharPtr sdpPtr(gst_sdp_message_as_text(sessionDescription->sdp));
    sdp = sdpPtr.get();

    prepared();
}

void GstClient::Private::onIceCandidate(
    GstElement* rtcbin,
    guint candidate,
    gchar* arg2)
{
    static std::string prefix("a=");
    iceCandidate(candidate, prefix + arg2);
}

void GstClient::Private::setState(GstState state)
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


GstClient::GstClient() :
    _p(new Private(this))
{
}

GstClient::~GstClient()
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_NULL);
}

void GstClient::prepare(
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

bool GstClient::sdp(std::string* sdp) noexcept
{
    if(!sdp)
        return false;

    if(_p->sdp.empty())
        return false;

    *sdp = _p->sdp;

    return true;
}

void GstClient::setRemoteSdp(const std::string& sdp) noexcept
{
    GstElement* rtcbin = _p->rtcbinPtr.get();

    auto onIceGatheringStateChangedCallback =
        (void (*) (GstElement*, GParamSpec* , gpointer))
        [] (GstElement* rtcbin, GParamSpec* paramSpec, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        return self->onIceGatheringStateChanged(rtcbin);
    };
    _p->iceGatheringStateChangedHandlerId =
        g_signal_connect(rtcbin,
            "notify::ice-gathering-state",
            G_CALLBACK(onIceGatheringStateChangedCallback), _p.get());

    GstSDPMessage* sdpMessage;
    gst_sdp_message_new(&sdpMessage);
    GstSDPMessagePtr sdpMessagePtr(sdpMessage);
    gst_sdp_message_parse_buffer(
        reinterpret_cast<const guint8*>(sdp.data()),
        sdp.size(),
        sdpMessage);

    GstWebRTCSessionDescriptionPtr sessionDescriptionPtr(
        gst_webrtc_session_description_new(
            GST_WEBRTC_SDP_TYPE_OFFER,
            sdpMessagePtr.release()));
    GstWebRTCSessionDescription* sessionDescription =
        sessionDescriptionPtr.get();

    g_signal_emit_by_name(_p->rtcbinPtr.get(),
        "set-remote-description", sessionDescription, NULL);

    auto onAnswerCreatedCallback =
        (void (*) (GstPromise*, gpointer))
        [] (GstPromise* promise, gpointer userData)
    {
        Private* self = static_cast<Private*>(userData);
        return self->onAnswerCreated(promise);
    };

    GstPromise* promise =
        gst_promise_new_with_change_func(
            onAnswerCreatedCallback,
            _p.get(), nullptr);
    g_signal_emit_by_name(
        _p->rtcbinPtr.get(), "create-answer", nullptr, promise);
}

void GstClient::addIceCandidate(
    unsigned mlineIndex,
    const std::string& candidate) noexcept
{
    GstWebRTCPeer::addIceCandidate(mlineIndex, candidate);
}

void GstClient::eos(bool error)
{
    if(_p->eos)
        _p->eos();
}

void GstClient::play() noexcept
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_PLAYING);
}

void GstClient::stop() noexcept
{
    if(_p->pipelinePtr)
        _p->setState(GST_STATE_NULL);
}
