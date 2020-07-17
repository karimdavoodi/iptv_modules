#include <boost/log/trivial.hpp>
#include "gst.hpp"
#include <thread>
#include <iostream>
#include "../third_party/json.hpp"

#define WAIT_MILISECOND(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))

using namespace std;
using nlohmann::json;

struct Mix_data {
    Gst::Data d;
    json config;
    bool video_pass;
    int video1_width;
    int video1_height;
    vector<GstPad*> audio_pads;
    Mix_data():d{},config{},video_pass{false},
            video1_width{0},video1_height{0},audio_pads{}{}
};
GstElement* insert_parser(GstPad* pad, Mix_data* tdata)
{
    auto caps = gst_pad_query_caps(pad, nullptr);
    auto caps_struct = gst_caps_get_structure(caps, 0);
    auto pad_type = string(gst_structure_get_name(caps_struct));

    GstElement* element = nullptr;
    if(pad_type.find("video/x-h264") != string::npos){
        element = Gst::add_element(tdata->d.pipeline, "h264parse", "", true);
        g_object_set(element, "config-interval", 1, nullptr);
    }else if(pad_type.find("video/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        LOG(debug) << "Mpeg version type:" <<  m_version;
        if(m_version == 4){
            element = Gst::add_element(tdata->d.pipeline, "mpeg4videoparse", "", true);
            g_object_set(element, "config-interval", 1, nullptr);
        }else{
            element = Gst::add_element(tdata->d.pipeline, "mpegvideoparse", "", true);
        }
    }else if(pad_type.find("audio/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        LOG(debug) << "Mpeg version type:" <<  m_version;
        if(m_version == 1){
            element = Gst::add_element(tdata->d.pipeline, "mpegaudioparse", "", true);
        }else{
            element = Gst::add_element(tdata->d.pipeline, "aacparse", "", true);
        }
    }else if(pad_type.find("audio/x-ac3") != string::npos ||
            pad_type.find("audio/ac3") != string::npos){
        element = Gst::add_element(tdata->d.pipeline, "ac3parse", "", true);
    }else{
        LOG(error) << "Type not support:" << pad_type;
    }
    if(element){
        g_object_set(element, "disable-passthrough", true, nullptr);
    }
    return element;
}
void connect_to_tsmux_mqueue(Mix_data* d, GstPad* pad)
{
    auto mqueue = gst_bin_get_by_name(GST_BIN(d->d.pipeline), "tsmux_mqueue");
    auto parser = insert_parser(pad, d);
    Gst::pad_link_element_static(pad, parser, "sink");
    Gst::element_link_request(parser, "src", mqueue, "sink_%u");
    gst_object_unref(mqueue);
}
GstPadProbeReturn parser_caps_probe(
        GstPad * pad, 
        GstPadProbeInfo * info, 
        gpointer data)
{
    auto d = (Mix_data *) data;
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

    if(GST_EVENT_TYPE (event) != GST_EVENT_CAPS) return GST_PAD_PROBE_OK;

    GstCaps* recvcaps = nullptr;
    gst_event_parse_caps (event, &recvcaps);
    if(recvcaps == nullptr) return GST_PAD_PROBE_OK;
    auto caps_struct = gst_caps_get_structure(recvcaps, 0);
    LOG(debug) << "Got caps event:" << Gst::caps_string(recvcaps);
    string name = gst_structure_get_name(caps_struct);

    if(name.find("video") != string::npos){
        int width = 0, height = 0;
        gst_structure_get_int(caps_struct, "width", &width);
        gst_structure_get_int(caps_struct, "height", &height);
        if(width && height){
            LOG(warning) << "Got dimension of Video1: " << width << "x" << height;
            d->video1_width = width;
            d->video1_height = height;
                // change last video dimension
            if(d->video1_width && d->video1_height){
                LOG(debug) << "Set Video Dimension: " << d->video1_width 
                    << "x" << d->video1_height;
                auto capsfilter = gst_bin_get_by_name(GST_BIN(d->d.pipeline), 
                        "video_caps");
                string caps_str = "video/x-raw, width=(int)" +
                    to_string(d->video1_width) + " , height=(int)" + 
                    to_string(d->video1_height);
                auto resize_caps = gst_caps_from_string(caps_str.c_str());
                g_object_set(capsfilter, "caps", resize_caps, nullptr);
                gst_caps_unref(resize_caps);
                gst_object_unref(capsfilter);
            }
            return GST_PAD_PROBE_REMOVE;
        } 
    }
    return GST_PAD_PROBE_OK;
}
GstElement* connect_to_parser_decoder_queue(Mix_data* d, GstPad* pad, string pad_type,
        bool detect_dimension = false)
{
    string decoder_name;
    auto caps = gst_pad_query_caps(pad, nullptr);
    auto caps_struct = gst_caps_get_structure(caps, 0);

    auto parser = insert_parser(pad, d);
    Gst::pad_link_element_static(pad, parser, "sink");

    if(detect_dimension){
        auto parse_src = gst_element_get_static_pad(parser, "src");
        gst_pad_add_probe(parse_src, GST_PAD_PROBE_TYPE_EVENT_BOTH,
                GstPadProbeCallback(parser_caps_probe), d, nullptr);

    }

    if(pad_type.find("video/x-h264") != string::npos){
        decoder_name = "avdec_h264";
    }else if(pad_type.find("video/x-h265") != string::npos){
        decoder_name = "avdec_h265";
    }else if(pad_type.find("video/mpeg") != string::npos){
        int mpegversion = 0;
        gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
        if(mpegversion == 4){ 
            decoder_name = "avdec_mpeg4";
        }else{
            decoder_name = "avdec_mpeg2video";
        }
    }else{
        LOG(error) << "Decoder not find for " << pad_type;
        g_main_loop_quit(d->d.loop);
        return nullptr;
    }
    auto decoder = Gst::add_element(d->d.pipeline, decoder_name, "", true);
    if(!decoder){
        LOG(error) << "Decoder not found:" << decoder_name;
        g_main_loop_quit(d->d.loop);
        return nullptr;
    }

    auto queue = Gst::add_element(d->d.pipeline, "queue", "", true);
    auto videoconvert = Gst::add_element(d->d.pipeline, "videoconvert", "", true);
    gst_element_link_many(parser, decoder, videoconvert, queue,  nullptr);
    return queue;
}
void ignore_pad(Mix_data* d, GstPad* pad)
{
    auto fake = Gst::add_element(d->d.pipeline, "fakesink", "", true);
    Gst::pad_link_element_static(pad, fake, "sink");
}
void tsdemux1_pad_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto d = (Mix_data *) data;
    auto element_name = Gst::element_name(object);
    auto pad_type = Gst::pad_caps_type(pad);

    LOG(debug) << "Elm:" << element_name << " type:" << pad_type;
    
    if(pad_type.find("audio") != string::npos){
        if(d->config["input1"]["useAudio"] == false ){
            LOG(debug) << "Ignore Audio1";
            return ignore_pad(d, pad);
        }
        // TODO: check audioNumber
        // TODO: do for pure audio stream
        if(d->video_pass){
            LOG(debug) << "Connect Audio1 pad to muxer";
            connect_to_tsmux_mqueue(d, pad);
        }else{
            d->audio_pads.push_back(pad);
            LOG(debug) << "Save Audio1 pad to connect later to muxer";
        } 
        
    }else if(pad_type.find("video") != string::npos){
        if(d->config["input1"]["useVideo"] == false ){
            LOG(debug) << "Ignore Video1";
            return ignore_pad(d, pad);
        }
        if(d->config["input2"]["useVideo"] == false ){
            LOG(debug) << "it's only video1 stream, connect pad to tsmux_mqueue";
            connect_to_tsmux_mqueue(d, pad);
        }else{
            LOG(debug) << "Connect to video1 -> decoder -> compositor ";  
            auto queue = connect_to_parser_decoder_queue(d, pad, pad_type, true);
            if(queue){
                auto compositor = gst_bin_get_by_name(GST_BIN(d->d.pipeline), "compositor");
                Gst::element_link_request(queue, "src", compositor, "sink_%u");
                gst_object_unref(compositor);
            }
        }
    }
}
void tsdemux2_pad_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto d = (Mix_data *) data;
    auto element_name = Gst::element_name(object);
    auto pad_type = Gst::pad_caps_type(pad);

    LOG(debug) << "Elm:" << element_name << " type:" << pad_type;
    
    if(pad_type.find("audio") != string::npos){
        if(d->config["input2"]["useAudio"] == false ){
            LOG(debug) << "Ignore Audio2";
            return ignore_pad(d, pad);
        }
        // TODO: check audioNumber
        if(d->video_pass){
            LOG(debug) << "Connect Audio2 pad to muxer";
            connect_to_tsmux_mqueue(d, pad);
        }else{
            d->audio_pads.push_back(pad);
            LOG(debug) << "Save Audio2 pad to connect later to muxer";
        } 
        
    }else if(pad_type.find("video") != string::npos){
        if(d->config["input2"]["useVideo"] == false ){
            LOG(debug) << "Ignore Video2";
            return ignore_pad(d, pad);
        }
        if(d->config["input1"]["useVideo"] == false ){
            LOG(debug) << "it's only video2 stream, connect pad to tsmux_mqueue";
            connect_to_tsmux_mqueue(d, pad);
        }else{
            LOG(debug) << "Connect to video2 -> decoder -> compositor ";  
            auto queue = connect_to_parser_decoder_queue(d, pad, pad_type);
            if(queue){
                auto compositor = gst_bin_get_by_name(GST_BIN(d->d.pipeline), "compositor");
                auto compositor_sink = gst_element_get_request_pad(compositor, "sink_%u");
                g_object_set(compositor_sink,
                        "zorder", 10,
                        "xpos",  d->config["input2"]["posX"].get<int>(),
                        "ypos",  d->config["input2"]["posY"].get<int>(),
                        "width", d->config["input2"]["width"].get<int>(),
                        "height",d->config["input2"]["height"].get<int>(),
                        nullptr);
                if(d->config["input2"]["whiteTransparent"]){
                    // Add alpha
                    auto alpha = Gst::add_element(d->d.pipeline, "alpha", "", true);
                    g_object_set(alpha,
                            "method",3,
                            "target-r",255,
                            "target-g",255,
                            "target-b",255,
                            "black-sensitivity",0,
                            "white-sensitivity",128,                           
                            nullptr);
                    gst_element_link_many(queue, alpha, nullptr);

                    auto alpha_src = gst_element_get_static_pad(alpha, "src");
                    if(gst_pad_link(alpha_src, compositor_sink) != GST_PAD_LINK_OK){
                        LOG(error) << "Can't link alpha to compositor";
                    }
                    gst_object_unref(alpha_src);
                }else{
                    auto queue_src = gst_element_get_static_pad(queue, "src");
                    if(gst_pad_link(queue_src, compositor_sink) != GST_PAD_LINK_OK){
                        LOG(error) << "Can't link queue to compositor";
                    }
                    gst_object_unref(queue_src);
                }
                gst_object_unref(compositor);
                gst_object_unref(compositor_sink);
            }
        }
    }
}
void multiqueue_pad_added(GstElement* multiqueue, GstPad* pad, gpointer data)
{
    if(GST_PAD_IS_SRC(pad)){
        auto d = (Mix_data *) data;
        d->video_pass = true;
        LOG(debug) << "Got src pad in multiqueue:" << Gst::pad_name(pad);
        auto mpegtsmux = gst_bin_get_by_name(GST_BIN(d->d.pipeline), "mpegtsmux");
        if(!Gst::pad_link_element_request(pad, mpegtsmux, "sink_%d")){
            LOG(error) << "Can't link  " << Gst::pad_name(pad) << " to mpegtsmux_sink";
            g_main_loop_quit(d->d.loop);
            return;
        }else LOG(debug) << "Link multiqueue:"<< Gst::pad_name(pad) << " to mpegtsmux";
        gst_object_unref(mpegtsmux);
        // Process Audio pads
        auto pad_name = Gst::pad_name(pad);
        if(pad_name == "src_0"){
            if(d->audio_pads.size()){
                LOG(debug) << "Connect Audio pads after Video pads num:" << d->audio_pads.size();
                for(auto pad : d->audio_pads){
                    connect_to_tsmux_mqueue(d, pad);
                }
                d->audio_pads.clear();
            }
        }
    }
}
/*
   Example config:
   {
   "_id": 1000671,
   "active": true,
   "name": "mixed1000671"
   "input1": {
   "input": 1000672,
   "inputType": 2,
   "useAudio": false,
   "useVideo": true,
   "audioNumber": 1
   },
   "input2": {
   "input": 1000671,
   "inputType": 2,
   "useAudio": true,
   "useVideo": true,
   "audioNumber": 1,
   "whiteTransparent": false,
   "posX": 0,
   "posY": 0,
   "height": 300,
   "width": 300
   }
   }

*  PIPELINE:
*
*
*   tsdemux1 -->   video -- if single use-----------------------> tsmux_mqueue
*                           if both   use--> dec -> queue_v1 -> MIX 
*                  audio -- if use -----------------------------> tsmux_mqueue
*
*   tsdemux2 -->   video -- if single use-----------------------> tsmux_mqueue
*                           if both   use--> dec -> queue_v2 -> MIX
*                  audio -- if use -----------------------------> tsmux_mqueue
*                              
*   MIX: queue_v1 ---------------------------> compositor   
*        queue_v2 --if transparent--> alpha -> compositor(pos,size)
*               --else -------------------->
*               queue -> enc -> tsmux_mqueue
* */
void gst_task(json config, string in_multicast1, string in_multicast2, string out_multicast, int port)
{
    in_multicast1 = "udp://" + in_multicast1 + ":" + to_string(port);
    in_multicast2 = "udp://" + in_multicast2 + ":" + to_string(port);

    if(!config["input1"]["useVideo"]  && !config["input2"]["useVideo"] ){
        LOG(error) << "You don't select Video stream! Exit.";
        return;
    }
    LOG(info) 
        << "Start " << in_multicast1 << " + " <<  in_multicast2 
        << " --> udp://" << out_multicast << ":" << port;

    Mix_data mdata;
    mdata.config = config;
    mdata.d.loop      = g_main_loop_new(nullptr, false);
    mdata.d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    try{
        auto udpsrc1       = Gst::add_element(mdata.d.pipeline, "udpsrc", "udpsrc1"),
             queue_src1    = Gst::add_element(mdata.d.pipeline, "queue", "queue_src1"),
             tsdemux1      = Gst::add_element(mdata.d.pipeline, "tsdemux", "tsdemux1"),

             udpsrc2       = Gst::add_element(mdata.d.pipeline, "udpsrc", "udpsrc2"),
             queue_src2    = Gst::add_element(mdata.d.pipeline, "queue", "queue_src2"),
             tsdemux2      = Gst::add_element(mdata.d.pipeline, "tsdemux", "tsdemux2"),

             multiqueue    = Gst::add_element(mdata.d.pipeline, "multiqueue", "tsmux_mqueue"),
             mpegtsmux    = Gst::add_element(mdata.d.pipeline, "mpegtsmux", "mpegtsmux"),
             queue_sink   = Gst::add_element(mdata.d.pipeline, "queue", "queue-sink"),
             udpsink      = Gst::add_element(mdata.d.pipeline, "udpsink");

        g_signal_connect(multiqueue, "pad-added", G_CALLBACK(multiqueue_pad_added), &mdata);
        g_signal_connect(tsdemux1,   "pad-added", G_CALLBACK(tsdemux1_pad_added), &mdata);
        g_signal_connect(tsdemux2,   "pad-added", G_CALLBACK(tsdemux2_pad_added), &mdata);

        if(config["input1"]["useVideo"]  && config["input2"]["useVideo"] ){
            auto compositor   = Gst::add_element(mdata.d.pipeline, "compositor", "compositor"),
                 queue0       = Gst::add_element(mdata.d.pipeline, "queue"),
                 videoconvert = Gst::add_element(mdata.d.pipeline, "videoconvert"),
                 videoscale   = Gst::add_element(mdata.d.pipeline, "videoscale"),
                 capsfilter   = Gst::add_element(mdata.d.pipeline, "capsfilter", "video_caps"),
                 queue1       = Gst::add_element(mdata.d.pipeline, "queue"),
                 x264enc      = Gst::add_element(mdata.d.pipeline, "x264enc"),
                 queue2       = Gst::add_element(mdata.d.pipeline, "queue"),
                 h264parse    = Gst::add_element(mdata.d.pipeline, "h264parse", "h264parse_x264");
            gst_element_link_many(compositor, queue0, videoconvert, videoscale, capsfilter, 
                    queue1, x264enc, queue2, h264parse, nullptr); 
            Gst::element_link_request(h264parse, "src", multiqueue, "sink_%u");
            g_object_set (x264enc, "speed-preset", 1, nullptr);
            g_object_set(h264parse, "disable-passthrough", true, nullptr);

            auto resize_caps = gst_caps_from_string(
                    "video/x-raw, width=(int)720 , height=(int)576");
            g_object_set(capsfilter, "caps", resize_caps, nullptr);
            gst_caps_unref(resize_caps);
            mdata.video_pass = true;
        } 
        gst_element_link_many(udpsrc1, queue_src1, tsdemux1, nullptr); 
        gst_element_link_many(udpsrc2, queue_src2, tsdemux2, nullptr); 
        gst_element_link_many(mpegtsmux, queue_sink, udpsink, nullptr); 


        g_object_set(udpsrc1, "uri", in_multicast1.c_str(), nullptr);
        g_object_set(udpsrc2, "uri", in_multicast2.c_str(), nullptr);
        g_object_set(mpegtsmux, "alignment", 7, nullptr); 
        g_object_set(queue_src1, "max-size-time", 2 * GST_SECOND, nullptr); 
        g_object_set(queue_src2, "max-size-time", 2 * GST_SECOND, nullptr); 
        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                "sync", true, 
                nullptr);
        Gst::add_bus_watch(mdata.d);
        Gst::dot_file(mdata.d.pipeline, "iptv_archive", 7);
        gst_element_set_state(GST_ELEMENT(mdata.d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(mdata.d.loop);

    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}

