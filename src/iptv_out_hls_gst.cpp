#include <thread>
#include "utils.hpp"
#include "gst.hpp"
using namespace std;

void tsdemux_pad_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto d = (Gst::Data*) data;
    auto caps = gst_pad_query_caps(pad, nullptr);
    auto caps_struct = gst_caps_get_structure(caps, 0);
    auto pad_type = string(gst_structure_get_name(caps_struct));

    LOG(debug) << Gst::pad_name(pad) << " Caps:" << Gst::caps_string(caps);
    GstElement* videoparse = nullptr;
    GstElement* audioparse = nullptr;
    if(pad_type.find("video/x-h264") != string::npos){
        videoparse = Gst::add_element(d->pipeline, "h264parse", "", true);
        g_object_set(videoparse, "config-interval", 1, nullptr);
    }else if(pad_type.find("video/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        LOG(debug) << "Mpeg version type:" <<  m_version;
        if(m_version == 4){
            videoparse = Gst::add_element(d->pipeline, "mpeg4videoparse", "", true);
            g_object_set(videoparse, "config-interval", 1, nullptr);
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
    gst_caps_unref(caps);
    // link tsdemux ---> queue --- > parse -->  qtmux
    auto parse = (videoparse != nullptr) ? videoparse : audioparse;
    auto parse_name = Gst::element_name(parse);
    if(parse != nullptr){
        g_object_set(parse, "disable-passthrough", true, nullptr);
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
        auto hlssink = gst_bin_get_by_name(GST_BIN(d->pipeline), "hlssink");
        string type = videoparse ? "video" : "audio";
        auto hls_pad = gst_element_get_request_pad(hlssink, type.c_str());
        if(hls_pad){
            auto parse_pad = gst_element_get_static_pad(parse, "src");
            if(gst_pad_link(parse_pad, hls_pad) != GST_PAD_LINK_OK){
                LOG(error) << "Can't link parse to hlssink";
            }
            gst_object_unref(hls_pad);
            gst_object_unref(parse_pad);
        }
        gst_object_unref(hlssink);
    }
}

void gst_task(string in_multicast, int port, string hls_root)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start " << in_multicast << " -> " << hls_root;

    Gst::Data d;
    d.loop      = g_main_loop_new(nullptr, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    try{
        auto udpsrc     = Gst::add_element(d.pipeline, "udpsrc"),
             queue_src  = Gst::add_element(d.pipeline, "queue", "queue_src"),
             tsdemux    = Gst::add_element(d.pipeline, "tsdemux"),
             hlssink    = Gst::add_element(d.pipeline, "hlssink2", "hlssink");

        gst_element_link_many(udpsrc, queue_src, tsdemux, nullptr);

        g_signal_connect(tsdemux, "pad-added", G_CALLBACK(tsdemux_pad_added), &d);
        g_object_set(udpsrc, "uri", in_multicast.c_str(), nullptr);
        string segment_location = hls_root + "/s__%05d.ts";
        string playlist_location = hls_root + "/p.m3u8";
        g_object_set(hlssink, 
                "max-files", 14,
                "playlist-length", 5,
                "playlist-location", playlist_location.c_str(),
                "location", segment_location.c_str(),
                "message-forward", true,
                "target-duration", 5,
                nullptr);

        Gst::add_bus_watch(d);
        Gst::dot_file(d.pipeline, "iptv_network", 5);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
