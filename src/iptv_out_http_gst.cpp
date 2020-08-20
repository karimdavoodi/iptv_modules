#include <thread>
#include "utils.hpp"
#include "gst.hpp"
#include <gst/video/video-event.h>
using namespace std;

void gst_task(string in_multicast, int port, int http_stream_port, const string ch_name)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start " << in_multicast << " -> http://IP:" << http_stream_port
        << "/" << ch_name << ".ts";

    Gst::Data d;
    d.loop      = g_main_loop_new(nullptr, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    try{
        auto udpsrc        = Gst::add_element(d.pipeline, "udpsrc"),
             queue_src     = Gst::add_element(d.pipeline, "queue", "queue_src"),
             tcpserversink = Gst::add_element(d.pipeline, "tcpserversink", "tcpserversink");

        gst_element_link_many(udpsrc, queue_src, tcpserversink, nullptr);
        g_object_set(udpsrc, "uri", in_multicast.c_str(), nullptr);
        g_object_set(tcpserversink,
                "sync-method",  3,                   
                "burst-format", 4,                  
                "burst-value",  2000,   //TODO: test for different platforms
                "buffers-min",  2000,
                "timeout",      3 * GST_SECOND,    
                "host",         "0.0.0.0",
                "port",         http_stream_port,
                nullptr);
        Gst::add_bus_watch(d);
        //Gst::dot_file(d.pipeline, "iptv_http", 5);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
