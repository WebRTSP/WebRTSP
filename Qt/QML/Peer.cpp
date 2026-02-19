#include "Peer.h"

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/webrtc/webrtc_fwd.h>

#include <CxxPtr/GstWebRtcPtr.h>


using namespace webrtsp::qml;

void Peer::prepare() noexcept
{
    GstWebRTCPeer::prepare(
        _webRTCConfig,
        [this] () { // receiverPrepared
            emit prepared(sdp());
        },
        [this] (unsigned mlineIndex, const std::string& candidate) { // iceCandidate
            emit iceCandidate(mlineIndex, candidate);
        },
        [this] () { // eos
            emit eos();
        },
        std::string());
}

// will be called from streaming thread
void Peer::postCanPlay(GstElement* element)
{
    GstStructure* structure = gst_structure_new_empty("can-play");
    GstMessage* message = gst_message_new_application(GST_OBJECT(element), structure);
    GstBusPtr busPtr(gst_element_get_bus(element));
    GstBus* bus = busPtr.get();
    gst_bus_post(bus, message);
}

void Peer::prepare(const WebRTCConfigPtr& webRTCConfig) noexcept
{
    GstElementPtr pipelinePtr(gst_pipeline_new("Client Pipeline"));
    GstElement* pipeline = pipelinePtr.get();

    GstElementPtr rtcbinPtr(gst_element_factory_make("webrtcbin", "clientrtcbin"));
    GstElement* rtcbin = rtcbinPtr.get();

    gst_bin_add_many(GST_BIN(pipeline), GST_ELEMENT(gst_object_ref(rtcbin)), NULL);

    {
        GstCapsPtr capsPtr(gst_caps_from_string("application/x-rtp, media=video"));
        GstWebRTCRTPTransceiver* recvonlyTransceiver = nullptr;
        g_signal_emit_by_name(
            rtcbin,
            "add-transceiver",
            GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY,
            capsPtr.get(),
            &recvonlyTransceiver);
        GstWebRTCRTPTransceiverPtr recvonlyTransceiverPtr(recvonlyTransceiver);
    }
    {
        GstCapsPtr capsPtr(gst_caps_from_string("application/x-rtp, media=audio"));
        GstWebRTCRTPTransceiver* recvonlyTransceiver = nullptr;
        g_signal_emit_by_name(
            rtcbin,
            "add-transceiver",
            GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY,
            capsPtr.get(),
            &recvonlyTransceiver);
        GstWebRTCRTPTransceiverPtr recvonlyTransceiverPtr(recvonlyTransceiver);
    }

    auto onPadAddedCallback =
        + [] (GstElement* webrtc, GstPad* pad, gpointer* userData) {
            Peer* self = reinterpret_cast<Peer*>(userData);
            GstElement* pipeline = self->pipeline();

            if(GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
                return;

            GstCapsPtr padCapsPtr(gst_pad_get_current_caps(pad));
            GstCapsPtr h264CapsPtr(gst_caps_from_string("application/x-rtp, media=video, encoding-name=H264"));
            GstCapsPtr vp8CapsPtr(gst_caps_from_string("application/x-rtp, media=video, encoding-name=VP8"));
            GstCapsPtr opusCapsPtr(gst_caps_from_string("application/x-rtp, media=audio, encoding-name=OPUS"));
            GstCaps* padCaps = padCapsPtr.get();
            GstCaps* h264Caps = h264CapsPtr.get();
            GstCaps* vp8Caps = vp8CapsPtr.get();
            GstCaps* opusCaps = opusCapsPtr.get();

            const gchar* decodeBinDescription = nullptr;
            bool video = true;
            if(gst_caps_is_always_compatible(padCaps, h264Caps)) {
                decodeBinDescription = "rtph264depay name=depay ! avdec_h264 ! videoconvert ! "
                    "queue ! glupload ! qml6glsink name=qmlsink";
            } else if(gst_caps_is_always_compatible(padCaps, vp8Caps)) {
                decodeBinDescription = "rtpvp8depay ! vp8dec ! videoconvert ! queue ! glupload ! qml6glsink name=qmlsink";
            } else if(gst_caps_is_always_compatible(padCaps, opusCaps)) {
                decodeBinDescription = "rtpopusdepay ! opusdec ! audioconvert ! queue ! autoaudiosink";
                video = false;
            }

            if(decodeBinDescription) {
                GstElement* decodeBin = gst_parse_bin_from_description(
                    decodeBinDescription,
                    TRUE,
                    nullptr);

                if(video) {
                    GstElementPtr sinkPtr(gst_bin_get_by_name(GST_BIN(decodeBin), "qmlsink"));
                    g_object_set(sinkPtr.get(), "widget", self->_view, nullptr);
                }
                gst_bin_add(GST_BIN(pipeline), decodeBin);
                gst_element_sync_state_with_parent(decodeBin);
                GstPad* sinkPad = (GstPad*)decodeBin->sinkpads->data;
                if(video) {
                    GstElementPtr depayPtr(gst_bin_get_by_name(GST_BIN(decodeBin), "depay"));
                    if(depayPtr) {
                        GstPadPtr depaySrcPadPtr(gst_element_get_static_pad(depayPtr.get(), "src"));
                        gst_pad_add_probe(
                            depaySrcPadPtr.get(),
                            GST_PAD_PROBE_TYPE_BUFFER,
                            [] (GstPad* pad, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
                                GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);

                                if(GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT))
                                    return GST_PAD_PROBE_DROP;
                                else {
                                    Peer::postCanPlay(GST_PAD_PARENT(pad));
                                    return GST_PAD_PROBE_REMOVE;
                                }
                            },
                            nullptr,
                            nullptr
                        );
                    }
                }
                gst_pad_link(pad, sinkPad);
            }
        };
    g_signal_connect(
        rtcbin,
        "pad-added",
        G_CALLBACK(onPadAddedCallback),
        this);

    setPipeline(std::move(pipelinePtr));
    setWebRtcBin(*webRTCConfig, std::move(rtcbinPtr));

    pause();
}

gboolean Peer::onBusMessage(GstMessage* message) noexcept
{
    if(GST_MESSAGE_TYPE(message) == GST_MESSAGE_APPLICATION) {
        if(gst_message_has_name(message, "can-play")) {
            emit canPlay();
            return TRUE;
        }
    }

    return GstWebRTCPeer::onBusMessage(message);
}
