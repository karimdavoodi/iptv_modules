#include <exception>
#include <thread>
#include <boost/log/trivial.hpp>
#include "gst.hpp"
#include "utils.hpp"
using namespace std;
struct ProbData {
    int buffer_count;
    GstBus* bus;
    GstElement* src; 
};
GstPadProbeReturn filesink_get_buffer(
        GstPad *pad,
        GstPadProbeInfo *info,
        gpointer user_data)
{
    auto d = (ProbData *)user_data;
    BOOST_LOG_TRIVIAL(debug) << "Got Buffer in filesink";
    d->buffer_count++;
    if(d->buffer_count > 1){
        gst_bus_post(d->bus, gst_message_new_eos(GST_OBJECT(d->src)));
        BOOST_LOG_TRIVIAL(debug) << "Post EOS message";
    }
    return GST_PAD_PROBE_OK;
}
void uridecodebin_pad_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto capsfilter = (GstElement*) data;
    BOOST_LOG_TRIVIAL(debug) << "uridecodebin pad caps:" << Gst::pad_caps_string(pad);
    auto pad_type = Gst::pad_caps_type(pad);
    if(pad_type.find("video") != string::npos)
        Gst::pad_link_element_static(pad, capsfilter, "sink");
}
bool gst_task(string in_multicast, int port, const string pic_path)
{
    Gst::Data d;
    d.loop = g_main_loop_new(NULL, false);
    d.pipeline = GST_PIPELINE(gst_element_factory_make("pipeline", NULL));
    BOOST_LOG_TRIVIAL(error) << "TEST0";
    LOG_AT(error) << "TEST1";
    return false;
    string uri = "udp://" + in_multicast + ":" + to_string(port);
    BOOST_LOG_TRIVIAL(info) << "Start " << uri << " --> " << pic_path;

    try{
        auto uridecodebin   = Gst::add_element(d.pipeline, "uridecodebin"),
             capsfilter     = Gst::add_element(d.pipeline, "capsfilter"),
             jpegenc        = Gst::add_element(d.pipeline, "jpegenc"),
             filesink       = Gst::add_element(d.pipeline, "filesink");
        
        gst_element_link_many(uridecodebin, capsfilter, jpegenc, filesink, NULL);
        
        g_object_set(uridecodebin, "uri", uri.c_str(), NULL);
        g_object_set(jpegenc, "snapshot", true, NULL);
        g_object_set(filesink, "location", pic_path.c_str(), NULL);
        auto caps = gst_caps_from_string("video/x-raw, format=(string)RGB");
        g_object_set(capsfilter, "caps", caps, NULL);
        gst_caps_unref(caps);

        g_signal_connect(uridecodebin, "pad-added",G_CALLBACK(uridecodebin_pad_added),
                                                capsfilter);

        
        Gst::add_bus_watch(d);
        ProbData pd { 0, d.bus, uridecodebin  };
        auto filesink_pad = gst_element_get_static_pad(filesink, "sink");
        gst_pad_add_probe(filesink_pad, GST_PAD_PROBE_TYPE_BUFFER, 
                GstPadProbeCallback(filesink_get_buffer), &pd, NULL);
        
        
        BOOST_LOG_TRIVIAL(debug) << "Wait to snapshot:" << SNAPSHOT_TIMEOUT; 
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
        
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_NULL);
        gst_object_unref(filesink_pad);
        return pd.buffer_count > 1 ;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
        return false;
    }
}


