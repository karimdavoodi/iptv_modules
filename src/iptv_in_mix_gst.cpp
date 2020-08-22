#include <boost/log/trivial.hpp>
#include "gst.hpp"
#include <thread>
#include <mutex>
#include <iostream>
#include "../third_party/json.hpp"
#define WAIT_MILISECOND(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))

using namespace std;
using nlohmann::json;

struct Mix_data {
    Gst::Data d;
    json config;
    int video1_width;
    int video1_height;
    bool video1, video2;
    vector<GstPad*> mqueue_src_pads;
    atomic_int tsdemux_src_pads_num;
    atomic_int tsdemux_no_more_pad;
    mutex mqueue_src_pads_mutex;

    Mix_data():d{},config{},
            video1_width{0},video1_height{0},
            video1{false},video2{false},mqueue_src_pads{},
            tsdemux_src_pads_num{0},tsdemux_no_more_pad{0}{}
};

void connect_to_tsmux_mqueue(Mix_data* d, GstPad* pad);
void ignore_pad(Mix_data* d, GstPad* pad);
void tsdemux1_pad_added(GstElement* object, GstPad* pad, gpointer data);
void tsdemux2_pad_added(GstElement* object, GstPad* pad, gpointer data);
void add_all_pads_to_mpegtsmux(Mix_data* tdata);
void multiqueue_pad_added(GstElement* multiqueue, GstPad* pad, gpointer data);
void tsdemux_no_more_pad1(GstElement* object, gpointer data);
void tsdemux_no_more_pad2(GstElement* object, gpointer data);

/*
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
/*
 *   The Gstreamer main function
 *   Mix to UDP Stream in One UDP stream.
 *  
 *   @param config: config of mixing
 *   @param in_multicast1 : UDP multicast of first input
 *   @param in_multicast2 : UDP multicast of second input
 *   @param out_multicast : UDP multicast of output
 *   @param port: multicast port numper of all stream
 *
 * */
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
        g_signal_connect(tsdemux1, "no-more-pads", G_CALLBACK(tsdemux_no_more_pad1), &mdata);
        g_signal_connect(tsdemux2, "no-more-pads", G_CALLBACK(tsdemux_no_more_pad2), &mdata);

        mdata.video1 = config["_profile"]["input1"]["useVideo"];
        mdata.video2 = config["_profile"]["input2"]["useVideo"];
        if(mdata.video1 && mdata.video2 ){
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

            string resize_caps_str = 
                    "video/x-raw, width=(int)" + 
                    to_string(config["_profile"]["output"]["width"]) +
                    " , height=(int)"+
                    to_string(config["_profile"]["output"]["height"]) ;
            auto resize_caps = gst_caps_from_string(resize_caps_str.c_str());
            g_object_set(capsfilter, "caps", resize_caps, nullptr);
            gst_caps_unref(resize_caps);
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
        Gst::dot_file(mdata.d.pipeline, "iptv_in_mix", 7);
        gst_element_set_state(GST_ELEMENT(mdata.d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(mdata.d.loop);

    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
void connect_to_tsmux_mqueue(Mix_data* d, GstPad* pad)
{
    auto mqueue = gst_bin_get_by_name(GST_BIN(d->d.pipeline), "tsmux_mqueue");
    auto parser = Gst::insert_parser(d->d.pipeline, pad);
    Gst::pad_link_element_static(pad, parser, "sink");
    Gst::element_link_request(parser, "src", mqueue, "sink_%u");
    gst_object_unref(mqueue);
}
GstElement* connect_to_parser_decoder_queue(Mix_data* d, GstPad* pad, string pad_type,
        bool detect_dimension = false)
{
    string decoder_name;
    auto caps = gst_pad_query_caps(pad, nullptr);
    auto caps_struct = gst_caps_get_structure(caps, 0);

    auto parser = Gst::insert_parser(d->d.pipeline, pad);
    Gst::pad_link_element_static(pad, parser, "sink");

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
        if(d->config["_profile"]["input1"]["useAudio"] == false ){
            LOG(debug) << "Ignore Audio1";
            return ignore_pad(d, pad);
        }
        // TODO: check audioNumber
        // TODO: do for pure audio stream
        LOG(debug) << "Connect Audio1 pad to muxer";
        connect_to_tsmux_mqueue(d, pad);
        d->tsdemux_src_pads_num++; 

    }else if(pad_type.find("video") != string::npos){
        if(d->video1 == false ){
            LOG(debug) << "Ignore Video1";
            return ignore_pad(d, pad);
        }
        d->tsdemux_src_pads_num++; 
        if(d->video2 == false ){
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
        if(d->config["_profile"]["input2"]["useAudio"] == false ){
            LOG(debug) << "Ignore Audio2";
            return ignore_pad(d, pad);
        }
        // TODO: check audioNumber
        connect_to_tsmux_mqueue(d, pad);
        d->tsdemux_src_pads_num++; 

    }else if(pad_type.find("video") != string::npos){
        if(d->video2 == false ){
            LOG(debug) << "Ignore Video2";
            return ignore_pad(d, pad);
        }
        d->tsdemux_src_pads_num++; 
        if(d->video1 == false ){
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
                        "xpos",  d->config["_profile"]["input2"]["posX"].get<int>(),
                        "ypos",  d->config["_profile"]["input2"]["posY"].get<int>(),
                        "width", d->config["_profile"]["input2"]["width"].get<int>(),
                        "height",d->config["_profile"]["input2"]["height"].get<int>(),
                        nullptr);
                if(d->config["_profile"]["input2"]["whiteTransparent"]){
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
void add_all_pads_to_mpegtsmux(Mix_data* tdata)
{
    std::unique_lock<mutex> lock(tdata->mqueue_src_pads_mutex);
    LOG(debug) << "NO MORE:" << tdata->tsdemux_no_more_pad << 
                " pad_num:" << tdata->tsdemux_src_pads_num
                << " vector size:" << tdata->mqueue_src_pads.size(); 

    int src_pads = tdata->tsdemux_src_pads_num;
    if(tdata->video1 && tdata->video2) 
        src_pads--;

    if(tdata->tsdemux_no_more_pad == 2 &&
            tdata->mqueue_src_pads.size() == (size_t) src_pads){
        auto mpegtsmux = gst_bin_get_by_name(GST_BIN(tdata->d.pipeline), "mpegtsmux");
        LOG(debug) << "Try to link all pads to mpegtsmux";
        for(auto p : tdata->mqueue_src_pads){
            if(!Gst::pad_link_element_request(p, mpegtsmux, "sink_%d")){
                LOG(error) << "Can't link  " << Gst::pad_name(p) << " to mpegtsmux_sink";
                g_main_loop_quit(tdata->d.loop);
                return;
            }else{
                LOG(debug) << "Link multiqueue"<< Gst::pad_name(p) << " to mpegtsmux";
            } 
        }
        gst_object_unref(mpegtsmux);
        tdata->mqueue_src_pads.clear();
    }

}
void multiqueue_pad_added(GstElement* multiqueue, GstPad* pad, gpointer data)
{
    if(GST_PAD_IS_SRC(pad)){
        LOG(debug) << "Got src pad in multiqueue:" << Gst::pad_name(pad);
        auto tdata = (Mix_data *) data;
        {
            std::unique_lock<mutex> lock(tdata->mqueue_src_pads_mutex);
            tdata->mqueue_src_pads.push_back(pad);
        }
        add_all_pads_to_mpegtsmux(tdata);
    }
}
void tsdemux_no_more_pad1(GstElement* object, gpointer data)
{
    auto tdata = (Mix_data *) data;
    tdata->tsdemux_no_more_pad += 1;
    LOG(debug) << "no more pad";
    add_all_pads_to_mpegtsmux(tdata);
}
void tsdemux_no_more_pad2(GstElement* object, gpointer data)
{
    auto tdata = (Mix_data *) data;
    tdata->tsdemux_no_more_pad += 1;
    LOG(debug) << "no more pad";
    add_all_pads_to_mpegtsmux(tdata);
}
