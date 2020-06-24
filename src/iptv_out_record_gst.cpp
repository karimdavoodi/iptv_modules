#include <thread>
#include "utils.hpp"
#include "gst.hpp"
#include <signal.h>
typedef void (*sighandler_t)(int);

bool stop_threads = false;
using namespace std;
void sig_handler(int signum)
{
    LOG(warning) << "Stop threads";
    stop_threads = true;
}
void tsdemux_pad_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto d = (Gst::Data*) data;
    auto caps_filter = gst_caps_new_any();
    auto caps = gst_pad_query_caps(pad, caps_filter);
    auto caps_struct = gst_caps_get_structure(caps, 0);
    auto caps_string = string(gst_caps_to_string(caps));
    auto pad_type = string(gst_structure_get_name(caps_struct));

    LOG(debug) << gst_pad_get_name(pad) << " Caps:" << caps_string;
    GstElement* videoparse = NULL;
    GstElement* audioparse = NULL;
    if(pad_type.find("video/x-h264") != string::npos){
        videoparse = Gst::add_element(d->pipeline, "h264parse", "", true);
        g_object_set(videoparse, "config-interval", 1, NULL);
    }else if(pad_type.find("video/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        LOG(debug) << "Mpeg version type:" <<  m_version;
        if(m_version == 4){
            videoparse = Gst::add_element(d->pipeline, "mpeg4videoparse", "", true);
            g_object_set(videoparse, "config-interval", 1, NULL);
        }else{
            videoparse = Gst::add_element(d->pipeline, "mpegvideoparse", "", true);
        }
    }else if(pad_type.find("audio/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        LOG(debug) << "Mpeg version type:" <<  m_version;
        if(m_version == 1){
            audioparse = Gst::add_element(d->pipeline, "mpegaudioparse", "", true);
        }else{
            audioparse = Gst::add_element(d->pipeline, "aacparse", "", true);
        }
    }else if(pad_type.find("audio/x-ac3") != string::npos ||
             pad_type.find("audio/ac3") != string::npos){
            audioparse = Gst::add_element(d->pipeline, "ac3parse", "", true);
    }else{
        LOG(warning) << "Not support:" << pad_type;
    }
    gst_caps_unref(caps_filter);
    gst_caps_unref(caps);
    // link tsdemux ---> queue --- > parse -->  qtmux
    auto parse = (videoparse != NULL) ? videoparse : audioparse;
    auto parse_name = string(gst_element_get_name(parse));
    if(parse != NULL){
        g_object_set(parse, "disable-passthrough", true, NULL);
        auto queue = Gst::add_element(d->pipeline, "queue", "", true);
        Gst::zero_queue_buffer(queue);
        if(!Gst::pad_link_element_static(pad, queue, "sink")){
            LOG(error) << "Can't link typefind to queue";
            g_main_loop_quit(d->loop); return; 
        }

        if(!gst_element_link(queue, parse)){
            LOG(error) << "Can't link  queue to " << parse_name; 
            g_main_loop_quit(d->loop); return;
        }
        auto qtmux = gst_bin_get_by_name(GST_BIN(d->pipeline), "qtmux");
        string sink_name = (videoparse != NULL) ? "video_%u" : "audio_%u";
        Gst::element_link_request(parse, "src", qtmux, sink_name.c_str());
        gst_object_unref(qtmux);
    }
}

void gst_task(string in_multicast, int port, string file_path, int second)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start " << in_multicast << " -> " << file_path 
        << " for " << second << " seconds";  
    Gst::Data d;
    d.loop      = g_main_loop_new(NULL, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", NULL));
    signal(15, sig_handler);
    try{
        auto udpsrc = Gst::add_element(d.pipeline, "udpsrc"),
             queue_src  = Gst::add_element(d.pipeline, "queue", "queue_src"),
             tsdemux    = Gst::add_element(d.pipeline, "tsdemux"),
             // add parser as dynamic
             //multiqueue = Gst::add_element(d.pipeline, "multiqueue", "multiqueue"),
             qtmux      = Gst::add_element(d.pipeline, "qtmux", "qtmux"),
             queue_sink = Gst::add_element(d.pipeline, "queue","queue_sink"),
             filesink   = Gst::add_element(d.pipeline, "filesink");

        gst_element_link_many(udpsrc, queue_src, tsdemux, NULL);
        gst_element_link_many(qtmux, queue_sink, filesink, NULL);

        g_signal_connect(tsdemux, "pad-added", G_CALLBACK(tsdemux_pad_added), &d);
        g_object_set(udpsrc, "uri", in_multicast.c_str(), NULL);
        g_object_set(filesink, "location", file_path.c_str(), NULL);

        Gst::add_bus_watch(d);
        std::thread t([&](){
                while(!stop_threads) Util::wait(100);
                if(d.bus)
                    gst_bus_post(d.bus, gst_message_new_eos(GST_OBJECT(udpsrc)));
                });
        //Gst::dot_file(d.pipeline, "iptv_network", 5);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_NULL);
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
