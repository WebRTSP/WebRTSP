#include "GstPipelineStreamer.h"

#include <cassert>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/webrtc/rtptransceiver.h>
#include <gst/webrtc/webrtc.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/GstPtr.h>

#include "LibGst.h"
#include "Helpers.h"


void GstPipelineStreamer::prepare()
{
    GError* parseError = nullptr;
    GstElementPtr pipelinePtr(gst_parse_launch(_pipeline.c_str(), &parseError));
    GErrorPtr parseErrorPtr(parseError);
    if(parseError)
        return;

    GstElement* pipeline = pipelinePtr.get();

    GstElementPtr rtcbinPtr;

    GstIterator* it = gst_bin_iterate_elements(GST_BIN(pipeline));
    GValue value = G_VALUE_INIT;
    while(gst_iterator_next(it, &value) == GST_ITERATOR_OK) {
        GstElement* element =
            static_cast<GstElement*>(g_value_get_object(&value));
        g_value_reset(&value);

        GstElementFactory* factory = gst_element_get_factory(element);
        if(0 == g_strcmp0(gst_plugin_feature_get_name(factory), "webrtcbin")) {
            gst_object_ref(element);
            rtcbinPtr.reset(element);
            break;
        }
    }
    gst_iterator_free(it);

    setPipeline(std::move(pipelinePtr));
    setWebRtcBin(std::move(rtcbinPtr));

    pause();
}

GstPipelineStreamer::GstPipelineStreamer(
    const std::string& pipeline) :
    _pipeline(pipeline)
{
}
