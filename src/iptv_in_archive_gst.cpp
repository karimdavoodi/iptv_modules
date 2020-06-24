#include <boost/log/trivial.hpp>
#include "gst.hpp"

using namespace std;
/*
 *  TODO: 
 *      - invalid timestamps in TS files 
 * */
void multiqueue_padd_added(GstElement* object, GstPad* pad, gpointer data)
{
    BOOST_LOG_TRIVIAL(debug) << "Multiqueue PAD:" << GST_PAD_NAME(pad);
    if(gst_pad_get_direction(pad) == GST_PAD_SRC ){
        auto d = (Gst::Data*) data;
        auto mpegtsmux = gst_bin_get_by_name(GST_BIN(d->pipeline), "mpegtsmux");
        auto mpegtsmux_pad_sink = gst_element_get_request_pad(mpegtsmux, "sink_%d");
        if(gst_pad_link(pad, mpegtsmux_pad_sink) != GST_PAD_LINK_OK){
            BOOST_LOG_TRIVIAL(error) << "Can't link multiqueue to mpegtsmux in pad:"
                <<  GST_PAD_NAME(pad) << " Caps:"<< Gst::pad_caps_string(pad);
        }
        gst_object_unref(mpegtsmux_pad_sink);
        gst_object_unref(mpegtsmux);
    }
}
void demux_padd_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto d = (Gst::Data*) data;
    auto caps_filter = gst_caps_new_any();
    auto caps = gst_pad_query_caps(pad, caps_filter);
    auto caps_struct = gst_caps_get_structure(caps, 0);
    auto caps_string = string(gst_caps_to_string(caps));
    auto pad_type = string(gst_structure_get_name(caps_struct));
    gst_caps_unref(caps_filter);
    gst_caps_unref(caps);

    BOOST_LOG_TRIVIAL(info) << "Demux add pad: " << gst_pad_get_name(pad)
                            << " Caps:" << caps_string;
    GstElement* videoparse = NULL;
    GstElement* audioparse = NULL;
    if(pad_type.find("video/x-h264") != string::npos){
        videoparse = Gst::add_element(d->pipeline, "h264parse", "", true);
        g_object_set(videoparse, "config-interval", 1, NULL);
    }else if(pad_type.find("video/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        BOOST_LOG_TRIVIAL(info) << "Mpeg version type:" <<  m_version;
        if(m_version == 4){
            videoparse = Gst::add_element(d->pipeline, "mpeg4videoparse", "", true);
            g_object_set(videoparse, "config-interval", 1, NULL);
        }else{
            videoparse = Gst::add_element(d->pipeline, "mpegvideoparse", "", true);
        }
    }else if(pad_type.find("audio/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        BOOST_LOG_TRIVIAL(info) << "Mpeg version type:" <<  m_version;
        if(m_version == 1){
            audioparse = Gst::add_element(d->pipeline, "mpegaudioparse", "", true);
        }else{
            audioparse = Gst::add_element(d->pipeline, "aacparse", "", true);
        }
    }else if(pad_type.find("audio/x-ac3") != string::npos ||
             pad_type.find("audio/ac3") != string::npos){
            audioparse = Gst::add_element(d->pipeline, "ac3parse", "", true);
    }else{
        BOOST_LOG_TRIVIAL(warning) << "Not support:" << pad_type;
    }
    // link typefind ---> parse --- > queue
    auto parse = (videoparse != NULL) ? videoparse : audioparse;
    auto parse_name = string(gst_element_get_name(parse));
    if(parse != NULL){
        g_object_set(parse, "disable-passthrough", true, NULL);
        if(!Gst::pad_link_element_static(pad, parse, "sink")){
            BOOST_LOG_TRIVIAL(error) << "Can't link typefind to " << parse_name;
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
    string file_type = gst_structure_to_string(caps_struct);
    BOOST_LOG_TRIVIAL(debug) << "Type find:" << file_type;
    GstElement* demux = NULL;
    bool is_mp3 = false;
    if(file_type.find("video/mpegts") != string::npos){
        demux = Gst::add_element(d->pipeline, "tsdemux", "demux");
    }else if(file_type.find("video/quicktime") != string::npos){
        demux = Gst::add_element(d->pipeline, "qtdemux", "demux");
    }else if(file_type.find("video/x-matroska") != string::npos){
        demux = Gst::add_element(d->pipeline, "matroskademux", "demux");
    }else if(file_type.find("audio/mpeg") != string::npos || 
             file_type.find("application/x-id3") != string::npos){
        demux = Gst::add_element(d->pipeline, "id3demux", "demux");
        is_mp3 = true;
    }else{
        BOOST_LOG_TRIVIAL(error) << "Type not support: " << file_type;
        g_main_loop_quit(d->loop);
        return;
    }
    if(demux != NULL){
        BOOST_LOG_TRIVIAL(info) << "Add demux";
        gst_element_link(typefind, demux);
        gst_element_set_state(demux, GST_STATE_PLAYING);
        if(is_mp3){
            auto audioparse = Gst::add_element(d->pipeline, "mpegaudioparse", "", true);
            auto queue = Gst::add_element(d->pipeline, "queue", "queue_mp3", true);
            Gst::zero_queue_buffer(queue);
            if(!gst_element_link_many(demux, audioparse, queue, NULL)){
                BOOST_LOG_TRIVIAL(error) << "Can't link demux-->audioparse-->queue";
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
        if(test_file!=NULL) media_path = string(test_file);
    }
    BOOST_LOG_TRIVIAL(info) 
        << "Start " << media_path 
        << " --> udp://" << multicast_addr << ":" << port;
    Gst::Data d;
    d.loop      = g_main_loop_new(NULL, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", NULL));

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

        gst_element_link_many(filesrc, queue_src, identity, typefind, NULL);
        gst_element_link_many(mpegtsmux, tsparse, queue ,udpsink, NULL);
    
        g_signal_connect(typefind, "have-type", G_CALLBACK(typefind_have_type), &d);
        g_signal_connect(multiqueue, "pad-added", G_CALLBACK(multiqueue_padd_added), &d);
        g_object_set(filesrc, 
                "location", media_path.c_str(), 
                NULL);
        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", multicast_addr.c_str() ,
                "port", port,
                "sync", true, NULL);
        g_object_set(mpegtsmux, "alignment", 7, NULL);
        g_object_set(tsparse, "set-timestamps", true, NULL);
        g_object_set(identity, "sync", true, NULL);

        Gst::add_bus_watch(d);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        Gst::dot_file(d.pipeline, "iptv_archive", 5);
        g_main_loop_run(d.loop);
        
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
    }
}
