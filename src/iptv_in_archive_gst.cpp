#include <boost/log/trivial.hpp>
#include "gst.hpp"

using namespace std;
/*
 *  TODO: 
 *      - invalid timestamps in TS files 
 * */
void multiqueue_padd_added(GstElement* object, GstPad* pad, gpointer data)
{
    LOG(debug) << "Multiqueue PAD:" << Gst::pad_name(pad);
    if(gst_pad_get_direction(pad) == GST_PAD_SRC ){
        auto d = (Gst::Data*) data;
        auto mpegtsmux = gst_bin_get_by_name(GST_BIN(d->pipeline), "mpegtsmux");
        auto mpegtsmux_pad_sink = gst_element_get_request_pad(mpegtsmux, "sink_%d");
        if(gst_pad_link(pad, mpegtsmux_pad_sink) != GST_PAD_LINK_OK){
            LOG(error) << "Can't link multiqueue to mpegtsmux in pad:"
                << Gst::pad_name(pad) << " Caps:"<< Gst::pad_caps_string(pad);
        }
        gst_object_unref(mpegtsmux_pad_sink);
        gst_object_unref(mpegtsmux);
    }
}
void demux_padd_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto d = (Gst::Data*) data;
    auto caps = gst_pad_query_caps(pad, nullptr);
    auto caps_struct = gst_caps_get_structure(caps, 0);
    auto pad_type = string(gst_structure_get_name(caps_struct));
    LOG(info) << " Caps:" << Gst::caps_string(caps);
    GstElement* videoparse = nullptr;
    GstElement* audioparse = nullptr;
    if(pad_type.find("video/x-h264") != string::npos){
        videoparse = Gst::add_element(d->pipeline, "h264parse", "", true);
        g_object_set(videoparse, "config-interval", 1, nullptr);
    }else if(pad_type.find("video/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        LOG(info) << "Mpeg version type:" <<  m_version;
        if(m_version == 4){
            videoparse = Gst::add_element(d->pipeline, "mpeg4videoparse", "", true);
            g_object_set(videoparse, "config-interval", 1, nullptr);
        }else{
            videoparse = Gst::add_element(d->pipeline, "mpegvideoparse", "", true);
        }
    }else if(pad_type.find("audio/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        LOG(info) << "Mpeg version type:" <<  m_version;
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
        if(!Gst::pad_link_element_static(pad, parse, "sink")){
            LOG(error) << "Can't link typefind to " << parse_name;
            g_main_loop_quit(d->loop);
            return;
        }
        auto multiqueue = gst_bin_get_by_name(GST_BIN(d->pipeline), "multiqueue");
        Gst::element_link_request(parse, "src", multiqueue, "sink_%u");
        gst_object_unref(multiqueue);
    }
}
void typefind_have_type(GstElement* typefind,
                                     guint arg0,
                                     GstCaps* caps,
                                     gpointer user_data)
{
    auto d = (Gst::Data*) user_data;
    auto caps_struct = gst_caps_get_structure(caps, 0);
    auto find_type = gst_structure_to_string(caps_struct);
    string type {find_type};
    g_free(find_type);
    LOG(debug) << "Type find:" << type;
    GstElement* demux = nullptr;
    bool is_mp3 = false;
    if(type.find("video/mpegts") != string::npos){
        demux = Gst::add_element(d->pipeline, "tsdemux", "demux");
    }else if(type.find("video/quicktime") != string::npos){
        demux = Gst::add_element(d->pipeline, "qtdemux", "demux");
    }else if(type.find("video/x-matroska") != string::npos){
        demux = Gst::add_element(d->pipeline, "matroskademux", "demux");
    }else if(type.find("audio/mpeg") != string::npos || 
             type.find("application/x-id3") != string::npos){
        demux = Gst::add_element(d->pipeline, "id3demux", "demux");
        is_mp3 = true;
    }else{
        LOG(error) << "Type not support: " << type;
        g_main_loop_quit(d->loop);
        return;
    }
    if(demux != nullptr){
        LOG(info) << "Add demux";
        gst_element_link(typefind, demux);
        gst_element_set_state(demux, GST_STATE_PLAYING);
        if(is_mp3){
            auto audioparse = Gst::add_element(d->pipeline, "mpegaudioparse", "", true);
            auto queue = Gst::add_element(d->pipeline, "queue", "queue_mp3", true);
            Gst::zero_queue_buffer(queue);
            if(!gst_element_link_many(demux, audioparse, queue, nullptr)){
                LOG(error) << "Can't link demux-->audioparse-->queue";
                g_main_loop_quit(d->loop);
                return;
            }
            auto mpegtsmux = gst_bin_get_by_name(GST_BIN(d->pipeline), "mpegtsmux");
            Gst::element_link_request(queue, "src", mpegtsmux, "sink_%d");
            gst_object_unref(mpegtsmux);
        }else{
            g_signal_connect(demux, "pad-added", G_CALLBACK(demux_padd_added), d);
        }
    }
}
void gst_task(string media_path, string multicast_addr, int port)
{
    {   // for test 
        char* test_file = getenv("GST_FILE_NAME");
        if(test_file!=nullptr) media_path = string(test_file);
    }
    LOG(info) 
        << "Start " << media_path 
        << " --> udp://" << multicast_addr << ":" << port;
    Gst::Data d;
    d.loop      = g_main_loop_new(nullptr, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));

    try{
        auto filesrc    = Gst::add_element(d.pipeline, "filesrc"),
             queue_src  = Gst::add_element(d.pipeline, "queue", "queue_src"),
             identity   = Gst::add_element(d.pipeline, "identity"),
             typefind   = Gst::add_element(d.pipeline, "typefind"),
             multiqueue = Gst::add_element(d.pipeline, "multiqueue", "multiqueue"),
             mpegtsmux  = Gst::add_element(d.pipeline, "mpegtsmux", "mpegtsmux"),
             tsparse    = Gst::add_element(d.pipeline, "tsparse"),
             queue      = Gst::add_element(d.pipeline, "queue","queue_sink"),
             udpsink    = Gst::add_element(d.pipeline, "udpsink");

        gst_element_link_many(filesrc, queue_src, identity, typefind, nullptr);
        gst_element_link_many(mpegtsmux, tsparse, queue ,udpsink, nullptr);
    
        g_signal_connect(typefind, "have-type", G_CALLBACK(typefind_have_type), &d);
        g_signal_connect(multiqueue, "pad-added", G_CALLBACK(multiqueue_padd_added), &d);
        g_object_set(filesrc, 
                "location", media_path.c_str(), 
                nullptr);
        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", multicast_addr.c_str() ,
                "port", port,
                "sync", true, nullptr);
        g_object_set(mpegtsmux, "alignment", 7, nullptr);
        g_object_set(tsparse, "set-timestamps", true, nullptr);
        g_object_set(identity, "sync", true, nullptr);

        Gst::add_bus_watch(d);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        Gst::dot_file(d.pipeline, "iptv_archive", 5);
        g_main_loop_run(d.loop);
        
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
