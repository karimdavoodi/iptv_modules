#include <exception>
#include <gst/gst.h>
#include <boost/log/trivial.hpp>
#include "config.hpp"
#include "utils.hpp"
using namespace std;

struct bus_and_bool {
    GstBus* bus;
    bool* first;
};
GstPadProbeReturn filesink_get_buffer(
        GstPad *pad,
        GstPadProbeInfo *info,
        gpointer user_data)
{
    auto _bus_and_bool = (bus_and_bool*)user_data;
    BOOST_LOG_TRIVIAL(trace) << "Got Buffer in filesink";
    if(! *( _bus_and_bool->first)){
        GstElement* parent = gst_pad_get_parent_element(pad);
        gst_bus_post(_bus_and_bool->bus, gst_message_new_eos(GST_OBJECT(parent)));
    }
    *_bus_and_bool->first = false;
    return GST_PAD_PROBE_OK;
}
bool gst_task(string in_multicast, int port, const string pic_path)
{
    GstElement* pipeline;
    try{
        GError* error = NULL;
        string uri = "udp://" + in_multicast + ":" + to_string(port);
        BOOST_LOG_TRIVIAL(info) << "Start " << uri << " => " << pic_path;
        //gst_debug_set_default_threshold(GST_LEVEL_INFO); 
        auto descr = g_strdup_printf (
                "uridecodebin uri=\"%s\" ! capsfilter caps=\"video/x-raw\" "
                " ! jpegenc snapshot=true ! filesink location=\"%s\" name=sink", 
                uri.c_str(), pic_path.c_str()); 
        BOOST_LOG_TRIVIAL(trace) << "PIPLINE:" << descr;
        pipeline = gst_parse_launch( descr, &error);
        if(error != NULL){
            BOOST_LOG_TRIVIAL(error) << "Not create pipeline:" << error->message;
            return false;
        }
        gst_element_set_state(pipeline, GST_STATE_PLAYING);

        auto filesink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
        auto filesink_pad = gst_element_get_static_pad(filesink, "sink");
        auto bus = gst_pipeline_get_bus( GST_PIPELINE(pipeline));
        bool first = true;
        auto _bus_and_bool = bus_and_bool{bus, &first};
        gst_pad_add_probe(filesink_pad, GST_PAD_PROBE_TYPE_BUFFER, 
                GstPadProbeCallback(filesink_get_buffer), &_bus_and_bool, NULL);
        GstMessage* msg;
        while(true){
            msg = gst_bus_pop(bus);
            if(msg == NULL) continue;
            if(msg->type == GST_MESSAGE_EOS){
                gst_message_unref(msg);
                break;
            } 
            gst_message_unref(msg);
        }
        gst_object_unref(filesink_pad);
        gst_object_unref(bus);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return true;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return false;
    }
}


