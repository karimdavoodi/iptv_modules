/*
 * Copyright (c) 2020 Karim, karimdavoodi@gmail.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <sstream>
#include <thread>
#include <vector>
#include "utils.hpp"
#include "gst.hpp"
using namespace std;
 
struct profile {
    string preset;
    string videoCodec;
    string videoSize;
    int videoRate = 0;
    int videoFps = 0;
    string videoProfile;  // TODO: apply it
    string audioCodec;
    int audioRate = 0;
};
struct transcoder_data {
    Gst::Data d;
    struct profile target;
    bool video_process;
    vector<GstPad*> mqueue_src_pads;
    atomic_int tsdemux_src_pads_num;
    bool tsdemux_no_more_pad_t;
    mutex mqueue_src_pads_mutex;

    transcoder_data():video_process{false},
        mqueue_src_pads{},tsdemux_src_pads_num{0},tsdemux_no_more_pad_t{false}{}
};


void queue_guard(GMainLoop* loop, GstElement* queue, int sec);
void multiqueue_pad_added_t(GstElement* multiqueue, GstPad* pad, gpointer data);
void to_multiqueue(GstPad* src_pad, transcoder_data* tdata);
void video_transcode(GstPad* src_pad, GstStructure* caps_struct, 
                                        transcoder_data* tdata);
void audio_transcode(GstPad* src_pad, GstStructure* caps_struct, 
                                        transcoder_data* tdata);
void process_audio_pad(GstPad* pad, transcoder_data* tdata);
void tsdemux_no_more_pad_t(GstElement* object, gpointer data);
void tsdemux_pad_added_t(GstElement* object, GstPad* pad, gpointer data);


/*
 *   The Gstreamer main function
 *   Ttranscoding of  udp:://in_multicast:port to udp:://out_multicast:port
 *   
 *   @param in_multicast : multicast of input stream
 *   @param out_multicast : multicast of output stream
 *   @param port: output multicast port numper 
 *   @param profile: config of Ttranscoding 
 *
 * */
void gst_transcode_of_stream(string in_multicast, int port, string out_multicast, json& profile)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) 
        << "Start  " << in_multicast 
        << " --> udp://" << out_multicast << ":" << port;

    transcoder_data tdata;
    tdata.d.loop      = g_main_loop_new(nullptr, false);
    tdata.d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));

    try{
        tdata.target.preset     = profile["preset"].is_null()? "" :
                                  profile["preset"];
        tdata.target.videoCodec = profile["videoCodec"].is_null() || 
                                  profile["videoCodec"] == "No Change" 
                                  ? "" : profile["videoCodec"];
        tdata.target.videoSize  = profile["videoSize"].is_null() ||
                                  profile["videoSize"] == "No Change" 
                                  ? "" : profile["videoSize"];
        tdata.target.videoFps   = !profile["videoFps"].is_number()? 0 :
                                  profile["videoFps"].get<int>();
        // Get in Mbps
        tdata.target.videoRate  = !profile["videoRate"].is_number()? 2000000 :
                                  profile["videoRate"].get<int>() * 1000000;
        tdata.target.videoProfile  = profile["videoProfile"].is_null()? "" :
                                  profile["videoProfile"];

        tdata.target.audioCodec = profile["audioCodec"].is_null() || 
                                  profile["audioCodec"] == "No Change" 
                                  ? "" : profile["audioCodec"];
        // Get in Kbps
        tdata.target.audioRate  = !profile["audioRate"].is_number()? 128000 :
                                  profile["audioRate"].get<int>() * 1000;

        // udpsrc -> tsdemux -> decoders -> encoders -> mpegtsmux -> udpsink
        auto udpsrc     = Gst::add_element(tdata.d.pipeline, "udpsrc"),
             queue_src  = Gst::add_element(tdata.d.pipeline, "queue", "queue_src"),
             tsdemux    = Gst::add_element(tdata.d.pipeline, "tsdemux"),
             multiqueue = Gst::add_element(tdata.d.pipeline, "multiqueue", "multiqueue"),
             mpegtsmux  = Gst::add_element(tdata.d.pipeline, "mpegtsmux", "mpegtsmux"),
             queue_sink = Gst::add_element(tdata.d.pipeline, "queue", "queue_sink"),
             udpsink    = Gst::add_element(tdata.d.pipeline, "udpsink");

        gst_element_link_many(udpsrc, queue_src, tsdemux, nullptr);
        gst_element_link_many(mpegtsmux, queue_sink, udpsink, nullptr);
        // Test it
        queue_guard(tdata.d.loop, queue_src, 20);

        g_signal_connect(tsdemux, "pad-added", G_CALLBACK(tsdemux_pad_added_t), &tdata);
        g_signal_connect(tsdemux, "no-more-pads", G_CALLBACK(tsdemux_no_more_pad_t), &tdata);
        g_signal_connect(multiqueue, "pad-added", G_CALLBACK(multiqueue_pad_added_t), &tdata);
        g_object_set(udpsrc, "uri", in_multicast.c_str(), nullptr);
        g_object_set(queue_sink,
                "max-size-buffers", 0,
                "max-size-bytes", 0,
                "max-size-time", 10 * GST_SECOND,
                nullptr);
        g_object_set(multiqueue,
                "sync-by-running-time", true,
                "unlinked-cache-time", 0,
                "max-size-buffers", 0,
                "max-size-bytes", 0,
                "max-size-time", 10 * GST_SECOND, 
                nullptr);
        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                //"sync", false,
                nullptr);
        g_object_set(mpegtsmux, "alignment", 7, nullptr);

        Gst::add_bus_watch(tdata.d);
        gst_element_set_state(GST_ELEMENT(tdata.d.pipeline), GST_STATE_PLAYING);
        Gst::dot_file(tdata.d.pipeline, "iptv_transcoder", 9);
        g_main_loop_run(tdata.d.loop);
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
}

void multiqueue_pad_added_t(GstElement* /*multiqueue*/, GstPad* pad, gpointer data)
{
    if(GST_PAD_IS_SRC(pad)){
        LOG(debug) << "Got src pad in multiqueue:" << Gst::pad_name(pad);
        auto tdata = (transcoder_data *) data;
        std::unique_lock<mutex> lock(tdata->mqueue_src_pads_mutex);
        
        tdata->mqueue_src_pads.push_back(pad);
        LOG(trace) << "tsdemux_no_more_pad_t:" << tdata-> tsdemux_no_more_pad_t
            << " mqueue_src_pads.size " << tdata->mqueue_src_pads.size()
            << " tsdemux_src_pads_num:" << tdata->tsdemux_src_pads_num;
        if(tdata->tsdemux_no_more_pad_t &&
           tdata->mqueue_src_pads.size() == (size_t)tdata->tsdemux_src_pads_num){
            auto mpegtsmux = gst_bin_get_by_name(GST_BIN(tdata->d.pipeline), "mpegtsmux");
            LOG(debug) << "Try to link all pads to mpegtsmux";
            for(auto p : tdata->mqueue_src_pads){
                if(!Gst::pad_link_element_request(p, mpegtsmux, "sink_%d")){
                    LOG(error) << "Can't link  " << Gst::pad_name(p) << " to mpegtsmux_sink";
                    g_main_loop_quit(tdata->d.loop);
                    return;
                }else{
                    LOG(debug) << "Link multiqueue "<< Gst::pad_name(p) << " to mpegtsmux";
                } 
            }
            gst_object_unref(mpegtsmux);
            tdata->mqueue_src_pads.clear();
        }
    }
}
void to_multiqueue(GstPad* src_pad, transcoder_data* tdata)
{
    LOG(trace) << "Link  " << Gst::pad_name(src_pad) << " -> multiqueue -> mpegtsmux";
    auto multiqueue = gst_bin_get_by_name(GST_BIN(tdata->d.pipeline), "multiqueue");
    Gst::pad_link_element_request(src_pad, multiqueue, "sink_%u");
    gst_object_unref(multiqueue);
}
void queue_overrun(GstElement* queue, gpointer user_data)
{
    GMainLoop* loop = (GMainLoop*) user_data;
    long current_level_bytes;
    g_object_get(queue, "current-level-bytes", &current_level_bytes, nullptr);
    LOG(error) << "Exit loop. queue overrun in Transcode: " << Gst::element_name(queue) 
        << " MB:" << current_level_bytes/1000'000L;
    g_main_loop_quit(loop);
}
void queue_guard(GMainLoop* loop, GstElement* queue, int sec)
{
    g_object_set(queue,
            "max-size-buffers", 0,
            "max-size-bytes", 0,
            "max-size-time", sec * 1000'000'000L, // in ns
            nullptr);
    g_signal_connect(queue, "overrun", G_CALLBACK( queue_overrun ), loop);
}
void video_transcode(GstPad* src_pad, GstStructure* caps_struct, transcoder_data* tdata)
{

    // Decode ...  
    string type = gst_structure_get_name(caps_struct);
    string decoder_name;
    LOG(debug) << "Input video type:" << type;

    if(type.find("video/x-h264") != string::npos){
        decoder_name = "avdec_h264";
    }else if(type.find("video/x-h265") != string::npos){
        decoder_name = "avdec_h265";
    }else if(type.find("video/mpeg") != string::npos){
        int mpegversion = 0;
        gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
        if(mpegversion == 4){ 
            decoder_name = "avdec_mpeg4";
        }else{
            decoder_name = "avdec_mpeg2video";
        }
    }
    if(decoder_name.empty()){
        LOG(error) << "Decoder not find for " << type;
        g_main_loop_quit(tdata->d.loop);
        return;
    }
    auto decoder = Gst::add_element(tdata->d.pipeline, decoder_name, "", true);
    if(!decoder){
        LOG(error) << "Decoder not found:" << decoder_name;
        g_main_loop_quit(tdata->d.loop);
        return;
    }
    Gst::pad_link_element_static(src_pad, decoder, "sink");

    // Resize , framerate  
    //   queue_raw -> videorate  -> video/x-raw, framerate=n/1 ->
    //                videoscale -> video/x-raw, width=x, heigth=y -> queue_resized 
    LOG(debug) << "Fps: " << tdata->target.videoFps << " Size:" << tdata->target.videoSize;
    auto [width, height] = Util::profile_resolution_pair(tdata->target.videoSize);
    auto caps_resize = Gst::add_element(tdata->d.pipeline, "capsfilter", "", true);
    auto caps_rate   = Gst::add_element(tdata->d.pipeline, "capsfilter", "", true);
    auto videoconvert1= Gst::add_element(tdata->d.pipeline, "videoconvert", "", true);
    auto videoscale  = Gst::add_element(tdata->d.pipeline, "videoscale", "", true);
    auto videorate   = Gst::add_element(tdata->d.pipeline, "videorate", "", true);
    auto queue_res   = Gst::add_element(tdata->d.pipeline, "queue", "", true);
    auto queue_res1   = Gst::add_element(tdata->d.pipeline, "queue", "", true);

    auto queue_raw = Gst::add_element(tdata->d.pipeline, "queue", "queue_raw", true);

    gst_element_link_many(decoder, queue_raw, videoconvert1, videorate, caps_rate,   
                        queue_res1, videoscale, caps_resize,queue_res, nullptr);
    // TODO: test it 
    queue_guard(tdata->d.loop, queue_raw, 20);
    std::ostringstream caps_str;
    if(width && height)
        caps_str << "video/x-raw, width=(int)" << width << ", height=(int)" << height;
    else
        caps_str << "video/x-raw";
    auto resize_caps = gst_caps_from_string(caps_str.str().c_str());
    g_object_set(caps_resize, "caps", resize_caps, nullptr);
    gst_caps_unref(resize_caps);

    std::ostringstream rate_str;
    if(tdata->target.videoFps)
        rate_str << "video/x-raw, framerate=(fraction)" << tdata->target.videoFps << "/1"; 
    else
        rate_str << "video/x-raw"; 
    auto rate_caps = gst_caps_from_string(rate_str.str().c_str());
    g_object_set(caps_rate, "caps", rate_caps, nullptr);
    gst_caps_unref(rate_caps);

    // Encode ...
    std::map<string, int> preset = {
        {"ultrafast", 1},
        {"fast", 5},
        {"medium", 6},
        {"slow", 7},
        {"veryslow",9 }
    };
    int preset_code = preset.count(tdata->target.preset) != 0 ?
        preset[tdata->target.preset] : 5;

    GstElement* encoder = nullptr;
    GstElement* parser = nullptr;
    if(tdata->target.videoCodec == "h264"){
        encoder = Gst::add_element(tdata->d.pipeline, "x264enc");
        parser = Gst::add_element(tdata->d.pipeline, "h264parse", "", true);
    }else if(tdata->target.videoCodec == "h265"){
        encoder = Gst::add_element(tdata->d.pipeline, "x265enc");
        parser = Gst::add_element(tdata->d.pipeline, "h265parse", "", true);
    }else if(tdata->target.videoCodec == "mpeg2"){
        encoder = Gst::add_element(tdata->d.pipeline, "mpeg2enc");
        parser = Gst::add_element(tdata->d.pipeline, "mpegvideoparse", "", true);
        g_object_set(encoder, "aspect", 1, nullptr);
    }
    if(!encoder){
        LOG(error) << "Encoder not found:" << tdata->target.videoCodec;
        g_main_loop_quit(tdata->d.loop);
        return;
    }

    g_object_set(encoder, "bitrate", (tdata->target.videoRate)/1000 , nullptr);  // in kbps
    if(tdata->target.videoCodec == "h264" || tdata->target.videoCodec == "h265"){
        g_object_set(encoder, 
                "speed-preset", preset_code, 
                "key-int-max", 30, 
                nullptr);
        /*
        // TODO: use user profile
        auto capsfilter = Gst::add_element(tdata->d.pipeline, "capsfilter", "hevc_caps");
        auto caps_str = (tdata->target.videoCodec == "h264") 
            ? "video/x-h264, profile=(string)baseline" : "video/x-h265, profile=(string)main";
        LOG(trace) << "Add caps for HEVC profile:" << caps_str;
        auto profile_caps = gst_caps_from_string(caps_str);
        g_object_set(capsfilter, "caps", profile_caps, nullptr);
        gst_element_link_many(queue_res, encoder, capsfilter, parser, nullptr);
        gst_caps_unref(profile_caps);
        */
        gst_element_link_many(queue_res, encoder, parser, nullptr);
    }else{
        gst_element_link_many(queue_res, encoder, parser, nullptr);
    }
    gst_element_set_state(encoder, GST_STATE_PLAYING);

    auto pad_src  = gst_element_get_static_pad(parser, "src");
    to_multiqueue(pad_src, tdata);
    gst_object_unref(pad_src);
}
void audio_transcode(GstPad* src_pad, GstStructure* caps_struct, transcoder_data* tdata)
{
    // Decode ...  
    int mpegversion = 0;
    int layer = 0;
    gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
    gst_structure_get_int(caps_struct, "layer", &layer);
    string type = gst_structure_get_name(caps_struct);
    string caps_str = gst_structure_to_string(caps_struct);
    string decoder_name;
    

    if(mpegversion == 1 && layer == 2){
        decoder_name = "avdec_mp2float";
    }else if(mpegversion == 1 && layer == 3){
        decoder_name = "avdec_mp3";
    }else if(mpegversion == 4  || mpegversion == 2){
        if(caps_str.find("loas") != string::npos)
            decoder_name = "avdec_aac_latm";
        else
            decoder_name = "avdec_aac";
    }else if(type.find("video/x-ac3")){
        decoder_name = "avdec_ac3";
    }
    if(decoder_name.empty()){
        LOG(error) << "Decoder not find for " << type;
        g_main_loop_quit(tdata->d.loop);
        return;
    }
    auto decoder = Gst::add_element(tdata->d.pipeline, decoder_name, "", true);
    if(!decoder){
        LOG(error) << "Decoder not found:" << decoder_name;
        g_main_loop_quit(tdata->d.loop);
        return;
    }
    Gst::pad_link_element_static(src_pad, decoder, "sink");

    // Encode ...
    
    int bitrate = tdata->target.audioRate;
    GstElement* encoder = nullptr;
    GstElement* parser = nullptr;
    if(tdata->target.audioCodec == "aac"){
        encoder = Gst::add_element(tdata->d.pipeline, "avenc_aac", "", true); 
        parser  = Gst::add_element(tdata->d.pipeline, "aacparse", "", true); 

    }else if(tdata->target.audioCodec == "mp2"){
        encoder = Gst::add_element(tdata->d.pipeline, "avenc_mp2", "", true);
        parser  = Gst::add_element(tdata->d.pipeline, "mpegaudioparse", "", true); 

    }else if(tdata->target.audioCodec == "mp3"){
        encoder = Gst::add_element(tdata->d.pipeline, "lamemp3enc", "", true);
        parser  = Gst::add_element(tdata->d.pipeline, "mpegaudioparse", "", true); 
        bitrate /=  1000;
    }
    if(!encoder){
        LOG(error) << "Encoder not found:" << tdata->target.audioCodec;
        g_main_loop_quit(tdata->d.loop);
        return;
    }
    auto audioconvert = Gst::add_element(tdata->d.pipeline, "audioconvert", "", true); 
    auto queue_raw = Gst::add_element(tdata->d.pipeline, "queue", "", true); 
    gst_element_link_many( decoder, queue_raw, audioconvert , encoder, parser, nullptr);

    LOG(trace) << "Set audio bitrate:" << bitrate;
    g_object_set(encoder, "bitrate", bitrate , nullptr);  

    auto pad_src  = gst_element_get_static_pad(parser, "src");
    to_multiqueue(pad_src, tdata);
    gst_object_unref(pad_src);
}
void process_audio_pad(GstPad* pad, transcoder_data* tdata)
{
    LOG(debug) << "audio pad: " << Gst::pad_name(pad) 
        << " caps:" << Gst::pad_caps_string(pad);
    auto caps = gst_pad_query_caps(pad, nullptr);
    auto caps_str = Gst::pad_caps_string(pad);
    auto caps_struct = gst_caps_get_structure(caps, 0);

    int mpegversion = 0;
    int layer = 0;
    gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
    gst_structure_get_int(caps_struct, "layer", &layer);
    /*
     *           mp3:     mpegversion: 1  layer: 3
     *           mp2:     mpegversion: 1  layer: 2
     *           aac:     mpegversion: 4  
     *           aac_loas:mpegversion: 4  string-format: loas 
     * */
    //transcode if codec name is diffirent
    bool transcode_audio = false;
    if(mpegversion == 1){
        if(tdata->target.audioCodec == "aac" || 
          (tdata->target.audioCodec == "mp2" && layer == 3) || 
          (tdata->target.audioCodec == "mp3" && layer == 2)){
            transcode_audio = true;
        }
    }
    if(mpegversion == 4 && tdata->target.audioCodec != "aac"){
        transcode_audio = true;
    }
    if(mpegversion != 4 && mpegversion != 1){
        transcode_audio = true;
    }
    if(mpegversion == 4 && caps_str.find("loas") != string::npos){
        transcode_audio = true;
    }
    if(transcode_audio && !tdata->target.audioCodec.empty()){
        LOG(info) << "Transcode audio due to codec name:" << tdata->target.audioCodec;
        audio_transcode(pad, caps_struct, tdata);
    }else{
        //passthrough audio
        LOG(info) << "Passthrough Audio due to same name:" << tdata->target.audioCodec;
        to_multiqueue(pad, tdata);
    }
    // TODO: check other parameters 
}
GstPadProbeReturn parser_caps_probe(
        GstPad * pad, 
        GstPadProbeInfo * info, 
        gpointer data)
{
    auto tdata = (transcoder_data *) data;
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

    if(GST_EVENT_TYPE (event) != GST_EVENT_CAPS) return GST_PAD_PROBE_OK;

    GstCaps* recvcaps = nullptr;
    gst_event_parse_caps (event, &recvcaps);
    if(recvcaps == nullptr) return GST_PAD_PROBE_OK;
    auto caps_struct = gst_caps_get_structure(recvcaps, 0);
    LOG(debug) << "Got caps event:" << Gst::caps_string(recvcaps);
    string name = gst_structure_get_name(caps_struct);

    if(name.find("video") != string::npos && !tdata->video_process){
        int width = 0, height = 0;
        gst_structure_get_int(caps_struct, "width", &width);
        gst_structure_get_int(caps_struct, "height", &height);
        if(!width || !height){
            LOG(warning) << "Video caps dosen't have size property";
            return GST_PAD_PROBE_OK;
        } 

        tdata->video_process = true;
        bool transcode_video = false;

        //transcode if codec name is diffirent
                
        if(name.find(tdata->target.videoCodec) == string::npos){
            LOG(info) << "Transcode Video due to codec name:" << tdata->target.videoCodec;
            transcode_video = true;
        }         
        //transcode if frame dimension is diffirent
        auto dst_resulotion   = Util::profile_resolution(tdata->target.videoSize);
        string src_resolution = to_string(width) + "x" + to_string(height);
        if(!dst_resulotion.empty() &&  src_resolution != dst_resulotion){
            LOG(info) << "Transcode Video due to frame size:" << dst_resulotion;
            transcode_video = true;
        }
        // TODO: check other parameters 
        if(!tdata->target.videoCodec.empty() && transcode_video){
            video_transcode(pad, caps_struct, tdata);
        }else{
            LOG(info) << "Passthrough Video due to same name and frame size";
            to_multiqueue(pad, tdata);
        }
        return GST_PAD_PROBE_OK;
    }
    if(name.find("audio/mpeg") != string::npos){
        int mpegversion = 0;
        int layer = 0;
        gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
        gst_structure_get_int(caps_struct, "layer", &layer);
        if(!mpegversion || (mpegversion == 1 && !layer)){
            LOG(warning) << "Audio caps dosen't have mpegversion property";
            return GST_PAD_PROBE_OK;
        }
        process_audio_pad(pad, tdata);
        return GST_PAD_PROBE_OK;
    }
    return GST_PAD_PROBE_OK;
}
void tsdemux_no_more_pad_t(GstElement* /*object*/, gpointer data)
{
    LOG(debug) << "No more pad";
    auto tdata = (transcoder_data *) data;
    tdata->tsdemux_no_more_pad_t = true;
}
void tsdemux_pad_added_t(GstElement* /*object*/, GstPad* pad, gpointer data)
{
    auto tdata = (transcoder_data *) data;

    LOG(debug) << Gst::pad_name(pad) << " Caps:" << Gst::pad_caps_string(pad);

    auto parse = Gst::insert_parser(tdata->d.pipeline, pad);
    if(parse != nullptr){
        auto parse_name = Gst::element_name(parse);
        LOG(trace) << "link tsdemux to " << parse_name;
        if(!Gst::pad_link_element_static(pad, parse, "sink")){
            LOG(error) << "Can't link tsdemux to " << parse_name;
            g_main_loop_quit(tdata->d.loop); return; 
        }

        auto parse_src = gst_element_get_static_pad(parse, "src");
        gst_pad_add_probe(parse_src, GST_PAD_PROBE_TYPE_EVENT_BOTH,
                GstPadProbeCallback(parser_caps_probe), data, nullptr);
        tdata->tsdemux_src_pads_num++;
    }
}
