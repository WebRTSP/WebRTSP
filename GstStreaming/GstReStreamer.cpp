#include "GstReStreamer.h"

#include <cassert>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/webrtc/rtptransceiver.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/GstPtr.h>

#include "Helpers.h"


GstReStreamer::GstReStreamer(const std::string& sourceUrl) :
    _sourceUrl(sourceUrl)
{
}

GstReStreamer::~GstReStreamer()
{
}

void GstReStreamer::prepare() noexcept
{
    assert(!pipeline());
    if(pipeline())
        return;

    _h264CapsPtr.reset(gst_caps_from_string("video/x-h264"));
    _vp8CapsPtr.reset(gst_caps_from_string("video/x-vp8"));

    GstCapsPtr supportedCapsPtr(gst_caps_copy(_h264CapsPtr.get()));
    gst_caps_append(supportedCapsPtr.get(), gst_caps_copy(_vp8CapsPtr.get()));
    GstCaps* supportedCaps = supportedCapsPtr.get();

    setPipeline(GstElementPtr(gst_pipeline_new(nullptr)));
    GstElement* pipeline = this->pipeline();

    GstElementPtr srcPtr(gst_element_factory_make("uridecodebin", nullptr));
    GstElement* decodebin = srcPtr.get();
    if(!decodebin)
        return;

    g_object_set(decodebin, "caps", supportedCaps, nullptr);

    auto srcPadAddedCallback =
        (void (*)(GstElement*, GstPad*, gpointer))
         [] (GstElement* decodebin, GstPad* pad, gpointer userData)
    {
        GstReStreamer* self = static_cast<GstReStreamer*>(userData);
        self->srcPadAdded(decodebin, pad);
    };
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(srcPadAddedCallback), this);

    auto noMorePadsCallback =
        (void (*)(GstElement*,  gpointer))
         [] (GstElement* decodebin, gpointer userData)
    {
        GstReStreamer* self = static_cast<GstReStreamer*>(userData);
        self->noMorePads(decodebin);
    };
    g_signal_connect(decodebin, "no-more-pads", G_CALLBACK(noMorePadsCallback), this);

    g_object_set(decodebin,
        "uri", _sourceUrl.c_str(),
        nullptr);

    gst_bin_add(GST_BIN(pipeline), srcPtr.release());

    play();
}

void GstReStreamer::srcPadAdded(
    GstElement* /*decodebin*/,
    GstPad* pad)
{
    if(webRtcBin())
        return;

    GstElement* pipeline = this->pipeline();

    GstCapsPtr capsPtr(gst_pad_get_current_caps(pad));
    GstCaps* caps = capsPtr.get();

    if(gst_caps_is_always_compatible(caps, _h264CapsPtr.get())) {
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
    } else if(gst_caps_is_always_compatible(caps, _vp8CapsPtr.get())) {
        GstElement* out =
            gst_parse_bin_from_description(
                "rtpvp8pay pt=96 ! "
                "webrtcbin name=srcrtcbin",
                TRUE, NULL);
        gst_bin_add(GST_BIN(pipeline), out);
        gst_element_sync_state_with_parent(out);

        GstPad *sink = (GstPad*)out->sinkpads->data;

        if(GST_PAD_LINK_OK != gst_pad_link(pad, sink))
            assert(false);
    } else
        return;

    setWebRtcBin(GstElementPtr(gst_bin_get_by_name(GST_BIN(pipeline), "srcrtcbin")));

    setIceServers();
}

void GstReStreamer::noMorePads(GstElement* /*decodebin*/)
{
    pause();
}
