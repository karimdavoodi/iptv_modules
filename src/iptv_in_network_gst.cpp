#include <boost/log/trivial.hpp>
#include "gst.hpp"
using namespace std;
/*
 *  TODO: 
 *
 * */
void urisourcebin_pad_added(GstElement* urisourcebin, GstPad* pad, gpointer data)
{
    auto d = (Gst::Data*) data;
    LOG(debug) << "Caps:" << Gst::pad_caps_string(pad);
    auto queue = gst_bin_get_by_name(GST_BIN(d->pipeline), "queue_src");
    if(!Gst::pad_link_element_static(pad, queue, "sink")){
        LOG(error) << "Can't link urisourcebin to queue by caps:" 
            <<  Gst::pad_caps_string(pad);
        g_main_loop_quit(d->loop);
    }
    gst_object_unref(queue);
}
void demux_padd_added(GstElement* object, GstPad* pad, gpointer data)
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
    // link typefind ---> parse --- > queue
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
        auto mpegtsmux = gst_bin_get_by_name(GST_BIN(d->pipeline), "mpegtsmux");
        Gst::element_link_request(parse, "src", mpegtsmux, "sink_%d");
        gst_object_unref(mpegtsmux);
    }
}
void typefind_have_type(GstElement* typefind,
                                     guint arg0,
                                     GstCaps* caps,
                                     gpointer user_data)
{
    auto d = (Gst::Data*) user_data;
    auto caps_struct = gst_caps_get_structure(caps, 0);
    string pad_type = string(gst_structure_get_name(caps_struct));
    auto struct_str = gst_structure_to_string(caps_struct);
    LOG(debug) << "Typefind:" << struct_str;
    g_free(struct_str);

    GstElement* demux = nullptr;
    bool is_mp3 = false;
    if(pad_type.find("video/mpegts") != string::npos){
        demux = Gst::add_element(d->pipeline, "tsdemux", "", true);
    }else if(pad_type.find("video/quicktime") != string::npos){
        demux = Gst::add_element(d->pipeline, "qtdemux", "", true);
    }else if(pad_type.find("video/x-matroska") != string::npos){
        demux = Gst::add_element(d->pipeline, "matroskademux", "", true);
    }else if(pad_type.find("audio/mpeg") != string::npos || 
             pad_type.find("application/x-id3") != string::npos){
        demux = Gst::add_element(d->pipeline, "id3demux", "", true);
        is_mp3 = true;
    }else if(pad_type.find("application/x-rtp") != string::npos){
        const char* encodin_name = gst_structure_get_string(caps_struct, "encoding-name");
        if(encodin_name == nullptr){
            LOG(error)<< "RTP dose not have encodin_name!";
            g_main_loop_quit(d->loop);
            return;
        }
        if(!strncmp(encodin_name, "MP2T", 4)){
            auto rtpmp2tdepay = Gst::add_element(d->pipeline, "rtpmp2tdepay", "", true);
            demux = Gst::add_element(d->pipeline, "tsdemux", "", true);
            gst_element_link_many(typefind, rtpmp2tdepay, demux, nullptr);
            g_signal_connect(demux, "pad-added", G_CALLBACK(demux_padd_added), d);
            return;
        }else{
            // TODO: check other types ... 
            LOG(error) << "Not support rtp type " << encodin_name;
            g_main_loop_quit(d->loop);
            return;
        }

    }else{
        LOG(error) << "Type not support: " << pad_type;
        g_main_loop_quit(d->loop);
        return;
    }
    if(demux != nullptr){
        gst_element_link(typefind, demux);
        if(is_mp3){
            auto audioparse = Gst::add_element(d->pipeline, "mpegaudioparse", "", true);
            auto queue = Gst::add_element(d->pipeline, "queue", "queue_mp3", true);
            if(!gst_element_link_many(demux, audioparse, queue, nullptr)){
                LOG(error) << "Can't link demux-->audioparse-->queue";
                g_main_loop_quit(d->loop);
            }
            auto mpegtsmux = gst_bin_get_by_name(GST_BIN(d->pipeline), "mpegtsmux");
            Gst::element_link_request(queue, "src", mpegtsmux, "sink_%d");
            gst_object_unref(mpegtsmux);
        }else{
            g_signal_connect(demux, "pad-added", G_CALLBACK(demux_padd_added), d);
        }
    }
}
void gst_task(string url, string multicast_addr, int port)
{
    LOG(info) 
        << "Start " << url 
        << " --> udp://" << multicast_addr << ":" << port;
    Gst::Data d;
    d.loop      = g_main_loop_new(nullptr, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));

    try{
        auto urisourcebin = Gst::add_element(d.pipeline, "urisourcebin"),
             queue_src  = Gst::add_element(d.pipeline, "queue", "queue_src"),
             typefind   = Gst::add_element(d.pipeline, "typefind"),
             mpegtsmux  = Gst::add_element(d.pipeline, "mpegtsmux", "mpegtsmux"),
             tsparse    = Gst::add_element(d.pipeline, "tsparse"),
             queue      = Gst::add_element(d.pipeline, "queue","queue_sink"),
             udpsink    = Gst::add_element(d.pipeline, "udpsink", "udpsink");

        gst_element_link_many(queue_src, typefind, nullptr);
        gst_element_link_many(mpegtsmux, queue, tsparse, udpsink, nullptr);

        g_signal_connect(urisourcebin, "pad-added", G_CALLBACK(urisourcebin_pad_added), &d);
        g_signal_connect(typefind, "have-type", G_CALLBACK(typefind_have_type), &d);
        g_object_set(urisourcebin, "uri", url.c_str(), nullptr);
        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", multicast_addr.c_str() ,
                "port", port,
                nullptr);
        g_object_set(mpegtsmux, "alignment", 7, nullptr);
        g_object_set(tsparse, "set-timestamps", true, nullptr);

        //Gst::dot_file(d.pipeline, "iptv_network", 5);
        Gst::add_bus_watch(d);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
