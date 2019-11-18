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

std::weak_ptr<LibGst> libGst;

struct GstStreamer::Private
{
    std::shared_ptr<LibGst> libGst;

    GstStreamer *const owner;

    PreparedCallback prepared;

    GstElementPtr pipelinePtr;

    GstBusPtr busPtr;
    guint busWatchId = 0;

    GstElementPtr rtcbinPtr;

    std::string sdp;

    void prepare();
    gboolean onBusMessage(GstBus*, GstMessage*);
    void onNegotiationNeeded(GstElement* rtcbin);
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
        "videotestsrc pattern=smpte100 ! "
        "x264enc ! video/x-h264, profile=baseline ! "
        "rtph264pay pt=96 !"
        "webrtcbin name=rtcbin";

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
        gst_bin_get_by_name(GST_BIN(pipeline), "rtcbin"));
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

void GstStreamer::Private::onNegotiationNeeded(GstElement* rtcBin)
{
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
        rtcBin, "create-offer", nullptr, promise);
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
    if(libGst.expired()) {
        _p->libGst = std::make_shared<LibGst>();
        libGst = _p->libGst;
    } else {
        _p->libGst = libGst.lock();
    }
}

GstStreamer::~GstStreamer()
{
}

void GstStreamer::prepare(const PreparedCallback& prepared) noexcept
{
    _p->prepared = prepared;

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

void GstStreamer::eos(bool error)
{
}

}
