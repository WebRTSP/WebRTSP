#include "GstTestStreamer.h"

#include <cassert>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/webrtc/rtptransceiver.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/GstPtr.h>

#include "LibGst.h"
#include "Helpers.h"


void GstTestStreamer::prepare()
{
    std::string usePattern = "smpte";
    if(_pattern == "bars")
        usePattern = "smpte100";
    else if(
        _pattern == "white" ||
        _pattern == "red" ||
        _pattern == "green" ||
        _pattern == "blue")
    {
        usePattern = _pattern;
    }

    const char* pipelineDesc;
    if(_videocodec == GstStreaming::Videocodec::h264) {
        pipelineDesc =
            "videotestsrc name=src ! "
            "x264enc ! video/x-h264, profile=baseline ! rtph264pay pt=96 ! "
            "webrtcbin name=srcrtcbin";
    } else {
        pipelineDesc =
            "videotestsrc name=src ! "
            "vp8enc ! rtpvp8pay pt=96 ! "
            "webrtcbin name=srcrtcbin";
    }

    GError* parseError = nullptr;
    GstElementPtr pipelinePtr(gst_parse_launch(pipelineDesc, &parseError));
    GErrorPtr parseErrorPtr(parseError);
    if(parseError)
        return;

    GstElement* pipeline = pipelinePtr.get();

    GstElementPtr srcPtr(gst_bin_get_by_name(GST_BIN(pipeline), "src"));
    GstElement* src = srcPtr.get();

    gst_util_set_object_arg(G_OBJECT(src), "pattern", usePattern.c_str());

    GstElementPtr rtcbinPtr(
        gst_bin_get_by_name(GST_BIN(pipeline), "srcrtcbin"));

    setPipeline(std::move(pipelinePtr));
    setWebRtcBin(std::move(rtcbinPtr));

    pause();
}

GstTestStreamer::GstTestStreamer(
    const std::string& pattern,
    GstStreaming::Videocodec videocodec) :
    _pattern(pattern), _videocodec(videocodec)
{
}
