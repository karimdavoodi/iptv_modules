#include <boost/log/trivial.hpp>
#include "gst.hpp"
#include <thread>
#include <iostream>
#include "../third_party/json.hpp"

#define WAIT_MILISECOND(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))

using namespace std;
using nlohmann::json;

void gst_task(json& channel, string out_multicast, int port)
{

    LOG(info) 
        << "Start " << "" 
        << " --> udp://" << out_multicast << ":" << port;

    Gst::Data d;
    d.loop      = g_main_loop_new(nullptr, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));

    try{
        auto appsrc       = Gst::add_element(d.pipeline, "appsrc"),
             queue_src    = Gst::add_element(d.pipeline, "queue", "queue_src"),
             videoconvert = Gst::add_element(d.pipeline, "videoconvert"),
             x264enc      = Gst::add_element(d.pipeline, "x264enc"),
             h264parse    = Gst::add_element(d.pipeline, "h264parse"),
             mpegtsmux    = Gst::add_element(d.pipeline, "mpegtsmux", "mpegtsmux"),
             rndbuffersize  = Gst::add_element(d.pipeline, "rndbuffersize","queue_sink"),
             udpsink      = Gst::add_element(d.pipeline, "udpsink");

        gst_element_link_many(
                appsrc, 
                queue_src, 
                videoconvert, 
                x264enc,
                h264parse,
                mpegtsmux,
                rndbuffersize,
                udpsink,
                nullptr);

        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                "sync", true, 
                nullptr);

        Gst::add_bus_watch(d);
        //Gst::dot_file(d.pipeline, "iptv_archive", 5);
        

        /////////////////////////////////////////// Start Pipline
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);

    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}

