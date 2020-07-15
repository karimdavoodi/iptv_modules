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
    Mix_data():d{},config{}{}
};
void connect_to_tsmux_mqueue(Mix_data* d, GstPad* pad)
{
    auto mqueue = gst_bin_get_by_name(GST_BIN(d->d.pipeline), "tsmux_mqueue");
    Gst::pad_link_element_request(pad, mqueue, "sink_%u");
    gst_object_unref(mqueue);
}
GstElement* connect_to_decoder_queue(Mix_data* d, GstPad* pad, string pad_type)
{
    string decoder_name;
    string parser_name;
    auto caps = gst_pad_query_caps(pad, nullptr);
    auto caps_struct = gst_caps_get_structure(caps, 0);
    if(pad_type.find("video/x-h264") != string::npos){
        decoder_name = "avdec_h264";
        parser_name = "h264parse";
    }else if(pad_type.find("video/x-h265") != string::npos){
        decoder_name = "avdec_h265";
        parser_name = "h265parse";
    }else if(pad_type.find("video/mpeg") != string::npos){
        int mpegversion = 0;
        gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
        if(mpegversion == 4){ 
            decoder_name = "avdec_mpeg4";
        }else{
            decoder_name = "avdec_mpeg2video";
        }
        parser_name = "mpegvideoparse";
    }else{
        LOG(error) << "Decoder not find for " << pad_type;
        g_main_loop_quit(d->d.loop);
        return nullptr;
    }
    auto decoder = Gst::add_element(d->d.pipeline, decoder_name, "", true);
    auto parser = Gst::add_element(d->d.pipeline, parser_name, "", true);
    if(!decoder || !parser){
        LOG(error) << "Decoder or parser not found:" << decoder_name;
        g_main_loop_quit(d->d.loop);
        return nullptr;
    }
    g_object_set(parser, "disable-passthrough", true, nullptr);
    auto queue = Gst::add_element(d->d.pipeline, "queue", "", true);
    Gst::pad_link_element_static(pad, parser, "sink");
    gst_element_link_many(parser, decoder, queue, nullptr);
    return queue;
}
void tsdemux1_pad_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto d = (Mix_data *) data;
    auto element_name = Gst::element_name(object);
    auto pad_type = Gst::pad_caps_type(pad);

    LOG(debug) << "Elm:" << element_name << " type:" << pad_type;
    
    if(pad_type.find("audio") != string::npos){
        if(d->config["input1"]["useAudio"] == false ) return;
        // connect pad to tsmux_mqueue
        // TODO: check audioNumber
        connect_to_tsmux_mqueue(d, pad);
        
    }else if(pad_type.find("video") != string::npos){
        if(d->config["input1"]["useVideo"] == false ) return;
        if(d->config["input2"]["useVideo"] == false ){
            // it's only video stream, connect pad to tsmux_mqueue
            connect_to_tsmux_mqueue(d, pad);
        }else{
            // connect to video mix:  decoder and queue_v1  
            auto queue = connect_to_decoder_queue(d, pad, pad_type);
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
        if(d->config["input2"]["useAudio"] == false ) return;
        // connect pad to tsmux_mqueue
        // TODO: check audioNumber
        connect_to_tsmux_mqueue(d, pad);
        
    }else if(pad_type.find("video") != string::npos){
        if(d->config["input2"]["useVideo"] == false ) return;
        if(d->config["input1"]["useVideo"] == false ){
            // it's only video stream, connect pad to tsmux_mqueue
            connect_to_tsmux_mqueue(d, pad);
        }else{
            // connect to video mix:  decoder and queue_v2  
            auto queue = connect_to_decoder_queue(d, pad, pad_type);
            if(queue){
                auto compositor = gst_bin_get_by_name(GST_BIN(d->d.pipeline), "compositor");
                auto compositor_sink = gst_element_get_request_pad(compositor, "sink_%u");
                g_object_set(compositor_sink,
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
        LOG(debug) << "Got src pad in multiqueue:" << Gst::pad_name(pad);
        auto d = (Mix_data *) data;
        auto mpegtsmux = gst_bin_get_by_name(GST_BIN(d->d.pipeline), "mpegtsmux");
        if(!Gst::pad_link_element_request(pad, mpegtsmux, "sink_%d")){
            LOG(error) << "Can't link  " << Gst::pad_name(pad) << " to mpegtsmux_sink";
            g_main_loop_quit(d->d.loop);
            return;
        }else LOG(debug) << "Link multiqueue"<< Gst::pad_name(pad) << " to mpegtsmux";
        gst_object_unref(mpegtsmux);
    }
}
void gst_task(json config, string in_multicast1, string in_multicast2, string out_multicast, int port)
{
    in_multicast1 = "udp://" + in_multicast1 + ":" + to_string(port);
    in_multicast2 = "udp://" + in_multicast2 + ":" + to_string(port);
    LOG(info) 
        << "Start " << in_multicast1 << " + " <<  in_multicast2 
        << " --> udp://" << out_multicast << ":" << port;

    Mix_data mdata;
    mdata.config = config;
    mdata.d.loop      = g_main_loop_new(nullptr, false);
    mdata.d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    /*
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
    try{
        auto udpsrc1       = Gst::add_element(mdata.d.pipeline, "udpsrc", "udpsrc1"),
             queue_src1    = Gst::add_element(mdata.d.pipeline, "queue", "queue_src1"),
             tsdemux1      = Gst::add_element(mdata.d.pipeline, "tsdemux", "tsdemux1"),

             udpsrc2       = Gst::add_element(mdata.d.pipeline, "udpsrc", "udpsrc2"),
             queue_src2    = Gst::add_element(mdata.d.pipeline, "queue", "queue_src2"),
             tsdemux2      = Gst::add_element(mdata.d.pipeline, "tsdemux", "tsdemux2"),


             multiqueue    = Gst::add_element(mdata.d.pipeline, "multiqueue", "tsmux_mqueue"),
             mpegtsmux    = Gst::add_element(mdata.d.pipeline, "mpegtsmux", "mpegtsmux"),
             udpsink      = Gst::add_element(mdata.d.pipeline, "udpsink");

        if(config["input1"]["useVideo"]  && config["input2"]["useVideo"] ){
             auto compositor   = Gst::add_element(mdata.d.pipeline, "compositor", "compositor"),
                  videoconvert = Gst::add_element(mdata.d.pipeline, "videoconvert"),
                  x264enc      = Gst::add_element(mdata.d.pipeline, "x264enc"),
                  h264parse    = Gst::add_element(mdata.d.pipeline, "h264parse");
             gst_element_link_many(compositor, videoconvert, x264enc, h264parse, nullptr); 
             auto h264parse_src = gst_element_get_static_pad(h264parse, "src");
             connect_to_tsmux_mqueue(&mdata, h264parse_src);
             g_object_set (x264enc, "speed-preset", 1, nullptr);
             g_object_set(h264parse, "disable-passthrough", true, nullptr);
        } 
        gst_element_link_many(udpsrc1, queue_src1, tsdemux1, nullptr); 
        gst_element_link_many(udpsrc2, queue_src2, tsdemux2, nullptr); 
        gst_element_link_many(mpegtsmux, udpsink, nullptr); 

        g_signal_connect(multiqueue, "pad-added", G_CALLBACK(multiqueue_pad_added), &mdata);
        g_signal_connect(tsdemux1, "pad-added", G_CALLBACK(tsdemux1_pad_added), &mdata);
        g_signal_connect(tsdemux2, "pad-added", G_CALLBACK(tsdemux2_pad_added), &mdata);

        g_object_set(udpsrc1, "uri", in_multicast1.c_str(), nullptr);
        g_object_set(udpsrc2, "uri", in_multicast2.c_str(), nullptr);
        g_object_set(mpegtsmux, "alignment", 7, nullptr); 
        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                "sync", true, 
                nullptr);

        Gst::add_bus_watch(mdata.d);
        //Gst::dot_file(mdata.d.pipeline, "iptv_archive", 5);
        gst_element_set_state(GST_ELEMENT(mdata.d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(mdata.d.loop);

    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}

