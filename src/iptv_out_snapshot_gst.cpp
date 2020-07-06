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
    LOG(debug) << "Got Buffer in filesink";
    d->buffer_count++;
    if(d->buffer_count > 1){
        gst_bus_post(d->bus, gst_message_new_eos(GST_OBJECT(d->src)));
        LOG(debug) << "Post EOS message";
    }
    return GST_PAD_PROBE_OK;
}
void uridecodebin_pad_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto d = (Gst::Data *)data;
    auto pad_type = Gst::pad_caps_type(pad);
    LOG(debug) << "pad type:" << pad_type;
    if(pad_type.find("video") != string::npos){
        LOG(debug) << "try to link to capsfilter";
        auto capsfilter = gst_bin_get_by_name(GST_BIN(d->pipeline), "capsfilter");
        Gst::pad_link_element_static(pad, capsfilter, "sink");
        gst_object_unref(capsfilter);
    }else{
        //auto fakesink = Gst::add_element(d->pipeline, "fakesink", "", true);
        //Gst::pad_link_element_static(pad, fakesink, "sink");
    }
}
bool gst_task(string in_multicast, int port, const string pic_path)
{
    Gst::Data d;
    d.loop = g_main_loop_new(nullptr, false);
    d.pipeline = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    string uri = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start " << uri << " --> " << pic_path;

    try{
        auto uridecodebin   = Gst::add_element(d.pipeline, "uridecodebin"),
             capsfilter     = Gst::add_element(d.pipeline, "capsfilter", "capsfilter"),
             //videoconvert   = Gst::add_element(d.pipeline, "videoconvert"),
             jpegenc        = Gst::add_element(d.pipeline, "jpegenc"),
             filesink       = Gst::add_element(d.pipeline, "filesink");
        
        gst_element_link_many(capsfilter, jpegenc, filesink, nullptr);
        
        g_object_set(uridecodebin, "uri", uri.c_str(), nullptr);
        //g_object_set(jpegenc, "snapshot", true, nullptr);
        g_object_set(filesink, "location", pic_path.c_str(), nullptr);
        auto caps = gst_caps_from_string("video/x-raw");
        g_object_set(capsfilter, "caps", caps, nullptr);
        gst_caps_unref(caps);

        g_signal_connect(uridecodebin, "pad-added",G_CALLBACK(uridecodebin_pad_added),
                                                &d);

        Gst::add_bus_watch(d);
        ProbData pd { 0, d.bus, uridecodebin  };
        auto filesink_pad = gst_element_get_static_pad(filesink, "sink");
        gst_pad_add_probe(filesink_pad, GST_PAD_PROBE_TYPE_BUFFER, 
                GstPadProbeCallback(filesink_get_buffer), &pd, nullptr);
        
        LOG(debug) << "Wait to snapshot:" << SNAPSHOT_TIMEOUT; 
        bool thread_running = true;
        std::thread t([&](){
                int counter = 60;
                while(thread_running && counter-- ) Util::wait(500);
                if(!d.pipeline || !d.loop || !thread_running) return;
                if(GST_OBJECT_REFCOUNT_VALUE(d.pipeline) == 0 ) return;
                LOG(debug) << "Pipeline timeout reached"; 
                g_main_loop_quit(d.loop);
                });
        t.detach();
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
        thread_running = false;

        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_NULL);
        gst_object_unref(filesink_pad);
        return pd.buffer_count > 1 ;
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
        return false;
    }
}


