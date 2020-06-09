#include <exception>
#include <thread>
#include <gst/gst.h>
#include <boost/log/trivial.hpp>
#include "config.hpp"
using namespace std;
/*
 *   TS file has timestamps, but other files dosen't have. 
 *
gst-launch-1.0 -m filesrc location=$TTS ! parsebin name=pars\
    ! queue ! video/x-h264 ! mpegtsmux name=mux !  udpsink host=229.5.5.5 port=3200 \
    pars. ! queue ! audio/mpeg ! mux.
 * */
bool parsebin_autoplug (GstElement  *bin,
                         GstPad     *pad,
                         GstElement *element,
                         GstQuery   *query,
                         gpointer   data)
{
    BOOST_LOG_TRIVIAL(trace) << __func__ ;
    BOOST_LOG_TRIVIAL(trace) << "Element1:" << gst_element_get_name(bin);
    BOOST_LOG_TRIVIAL(trace) << "Element2:" << gst_element_get_name(element);
    BOOST_LOG_TRIVIAL(trace) << "Pad:" << gst_pad_get_name(pad);
    auto q_struct = gst_query_get_structure(query);
    BOOST_LOG_TRIVIAL(trace) << "Query:" <<  gst_structure_to_string(q_struct);
    
    return true;
}
bool element_link_request(GstElement* src, const char* src_name, 
            GstElement* sink, const char* sink_name)
{
        auto pad_sink = gst_element_get_request_pad(sink, sink_name);
        auto pad_src  = gst_element_get_static_pad(src, src_name);
        if(!pad_sink || !pad_src){
            throw std::runtime_error("Can't get pads in " + string( __func__));
        }
        if(gst_pad_link(pad_src, pad_sink) != GST_PAD_LINK_OK){
            gst_object_unref(pad_sink);
            gst_object_unref(pad_src);
            throw std::runtime_error("Can't link pads in " + string( __func__));
        }
        gst_object_unref(pad_sink);
        gst_object_unref(pad_src);
        return true;
}
void parsebin_pad_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto pipeline = (GstElement*) data;
    auto caps_filter = gst_caps_new_any();
    auto caps = gst_pad_query_caps(pad, caps_filter);
    auto caps_struct = gst_caps_get_structure(caps, 0);
    auto caps_string = string(gst_caps_to_string(caps));
    auto pad_type = string(gst_structure_get_name(caps_struct));
    gst_caps_unref(caps_filter);
    gst_caps_unref(caps);
    char* pad_name = gst_pad_get_name(pad);
    BOOST_LOG_TRIVIAL(info) << "ParseBin add pad: " << pad_name
                            << " ###### Caps:" << caps_string;
    GstElement* queue;
    if(pad_type.find("video") != string::npos)
        queue =  gst_bin_get_by_name(GST_BIN(pipeline), "queue_v");
    else if(pad_type.find("audio") != string::npos)
        queue =  gst_bin_get_by_name(GST_BIN(pipeline), "queue_a");
    else return;
    auto queue_src = gst_element_get_static_pad(queue, "sink");
    if(gst_pad_link(pad, queue_src) != GST_PAD_LINK_OK){
        BOOST_LOG_TRIVIAL(error) << "pase->queue link error on type: " << pad_type; 
    }
    gst_object_unref(queue_src);
    gst_object_unref(queue);
}
void gst_task(string media_path, string multicast_addr, int port)
{
        media_path = "/home/karim/Music/test_h264_aac.mp4";
        BOOST_LOG_TRIVIAL(info) 
            << "Start " 
            << media_path 
            << " --> udp://" 
            << multicast_addr 
            << ":" << port;
        gst_debug_set_default_threshold(GST_LEVEL_DEBUG); 
        auto loop = g_main_loop_new(NULL, false);
        auto pipeline   = gst_element_factory_make("pipeline","pipeline");
        auto filesrc    = gst_element_factory_make("filesrc","filesrc");
        auto queue_big  = gst_element_factory_make("queue", "queue_big");
        auto parsebin   = gst_element_factory_make("parsebin", "parse");
        auto queue_v    = gst_element_factory_make("queue", "queue_v");
        auto queue_a    = gst_element_factory_make("queue", "queue_a");
        auto queue_end  = gst_element_factory_make("queue", "queue_end");
        auto mpegtsmux  = gst_element_factory_make("avmux_mpegts", "mux");
        auto udpsink    = gst_element_factory_make("udpsink", "vSink");
        if( !filesrc || !parsebin || !udpsink ){
            BOOST_LOG_TRIVIAL(debug) << "Error in Element create";
            return;
        }
    try{
        gst_bin_add_many(GST_BIN(pipeline), 
                filesrc    ,
                queue_big  ,
                parsebin   ,
                queue_v    ,
                queue_a    ,
                queue_end  ,
                mpegtsmux  ,
                udpsink    ,
                NULL);
        gst_element_link_many(filesrc, queue_big, parsebin, NULL);
        element_link_request(queue_v, "src", mpegtsmux, "video_%u");
        element_link_request(queue_a, "src", mpegtsmux, "audio_%u");
        gst_element_link_many(mpegtsmux, queue_end, udpsink, NULL);
        
        g_signal_connect(parsebin, "autoplug-query", G_CALLBACK(parsebin_autoplug),NULL);
        g_signal_connect(parsebin, "pad_added", G_CALLBACK(parsebin_pad_added), pipeline);
        //gst_base_src_set_live(GST_BASE_SRC(filesrc->gobj()), true);
        g_object_set(filesrc, "location", media_path.c_str(), NULL);
        g_object_set(parsebin, "message-forward", true, NULL);
        g_object_set(udpsink, "multicast-iface", "lo", NULL);
        g_object_set(udpsink, "host", multicast_addr.c_str(), NULL);
        g_object_set(udpsink, "port", port, NULL);
        g_object_set(udpsink, "sync", true, NULL);
        //PIPLINE_WATCH;
        //PIPLINE_POSITION;
        //PIPLINE_DOT_FILE;
        
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        g_main_loop_run(loop);
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
    }
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_object_unref(loop);
    BOOST_LOG_TRIVIAL(debug) << "Finish";
}
