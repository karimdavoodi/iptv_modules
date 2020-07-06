#include <exception>
#include <thread>
#include "gst.hpp"
using namespace std;
void gst_task(string  in_multicast, string out_multicast, int port)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start " << in_multicast << " -> udp://" << out_multicast << ":" << port;

    Gst::Data d;
    d.loop      = g_main_loop_new(nullptr, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));

    try{
        auto udpsrc         = Gst::add_element(d.pipeline, "udpsrc"),
             queue          = Gst::add_element(d.pipeline, "queue"),
             rndbuffersize  = Gst::add_element(d.pipeline, "rndbuffersize"),
             udpsink        = Gst::add_element(d.pipeline, "udpsink");

        gst_element_link_many(udpsrc, queue, rndbuffersize, udpsink, nullptr);
        
        g_object_set(udpsrc, "uri", in_multicast.c_str(), nullptr);
        g_object_set(rndbuffersize, "min", 1316, "max", 1316, nullptr);

        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                "sync", true, nullptr);
        
        Gst::add_bus_watch(d);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
