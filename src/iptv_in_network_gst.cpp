#include <exception>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <boost/log/trivial.hpp>
#include "gst.hpp"
/*
            urisourcebin ---> queue1 -->  parsebin 
                        src_0 --> queue  --> mpegtsmux.video --> 
                        src_1 --> queue  --> mpegtsmux.audio -->  
                                       queue2 --> rndbuffersize ---> udpsink
 * */
using namespace std;
void parsebin_pad_added(GstElement* elem, GstPad* pad, gpointer pipeline)
{
    auto query_caps = gst_caps_new_any();
    auto pad_caps = gst_pad_query_caps(pad, query_caps); 
    auto pad_struct = gst_caps_get_structure(pad_caps, 0);
    string name = gst_structure_get_name(pad_struct);
    BOOST_LOG_TRIVIAL(trace) << gst_caps_to_string(pad_caps);
    gst_caps_unref(query_caps);
    gst_caps_unref(pad_caps);

    GstElement* queue = NULL;
    if(name.find("video") != string::npos){
        queue = gst_bin_get_by_name(GST_BIN(pipeline), "queue_video");
    }else if(name.find("audio") != string::npos){
        queue = gst_bin_get_by_name(GST_BIN(pipeline), "queue_audio");
    }
    if(queue != NULL){
        auto queue_sink_pad = gst_element_get_static_pad(GST_ELEMENT(queue), "sink");
        if(gst_pad_link(pad, queue_sink_pad) != GST_PAD_LINK_OK){
            BOOST_LOG_TRIVIAL(error) << "Can't link parsebin to queue_src";
        }else{
            BOOST_LOG_TRIVIAL(debug) << "Link parsebin to queue type:" << name;
        }
        gst_object_unref(queue_sink_pad);
    }else{
        BOOST_LOG_TRIVIAL(warning) << "Not link parsebin src pad " 
            << gst_pad_get_name(pad) << " type:" << name;
    }
}
void urisourcebin_pad_added(GstElement* elem, GstPad* pad, gpointer queue_src)
{
    auto queue_sink_pad = gst_element_get_static_pad(GST_ELEMENT(queue_src), "sink");
    if(gst_pad_link(pad, queue_sink_pad) != GST_PAD_LINK_OK){
        BOOST_LOG_TRIVIAL(error) << "Can't link urisourcebin to queue_src";
    }else{
        BOOST_LOG_TRIVIAL(trace) << "Link urisourcebin to queue_src";
    }
    gst_object_unref(queue_sink_pad);
}
void gst_task(string in_url, string out_multicast, int port)
{
    BOOST_LOG_TRIVIAL(info) << in_url << " --> " 
        << out_multicast << ":" << port;
    auto loop = g_main_loop_new(NULL, false);
    auto pipeline = gst_element_factory_make("pipeline", "pipeline");
    try{
        
        auto urisourcebin   = Gst::add_element(pipeline, "urisourcebin"),
             queue_src      = Gst::add_element(pipeline, "queue", "queue_src"),
             parsebin       = Gst::add_element(pipeline, "parsebin"),
             queue_video    = Gst::add_element(pipeline, "queue", "queue_video"),
             queue_audio    = Gst::add_element(pipeline, "queue", "queue_audio"),
             mpegtsmux      = Gst::add_element(pipeline, "mpegtsmux"),
             rndbuffersize  = Gst::add_element(pipeline, "rndbuffersize"),
             udpsink        = Gst::add_element(pipeline, "udpsink");

        gst_element_link_many(queue_src, parsebin, NULL);
        Gst::element_link_request(queue_video, "src", mpegtsmux, "sink_%d");
        Gst::element_link_request(queue_audio, "src", mpegtsmux, "sink_%d");
        gst_element_link_many(mpegtsmux, rndbuffersize, udpsink, NULL);

        g_object_set(urisourcebin, "uri",  in_url.c_str(), NULL);
        g_object_set(udpsink, "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                "sync", true, NULL);
        g_object_set(rndbuffersize, 
                "min", 1316,
                "max", 1316, NULL);

        g_signal_connect(urisourcebin, "pad-added", 
                G_CALLBACK(urisourcebin_pad_added), queue_src);
        g_signal_connect(parsebin, "pad-added", 
                G_CALLBACK(parsebin_pad_added), pipeline);

        //Gst::add_bus_watch(pipeline, loop);
        Gst::dot_file(pipeline, "iptv_in_network", 5);

        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        g_main_loop_run(loop);
        BOOST_LOG_TRIVIAL(info) << "Finish";
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
    }
    g_main_loop_unref(loop);
    gst_object_unref(pipeline);
}
