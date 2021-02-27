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
// TODO: enable multi audio HLS 
#include <boost/filesystem/operations.hpp>
#include <chrono>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gst/gstelement.h"
#include "utils.hpp"
#include "gst.hpp"
using namespace std;

struct Hdata {
    Gst::Data d;
    char chan_name[255];
    bool added_audio;
    bool added_video;
};
void tsdemux_pad_added_h(GstElement* object, GstPad* pad, gpointer data);
void queue_guard(Hdata* h, GstElement* queue, int sec);
/*
 *   The Gstreamer main function
 *   Convert udp:://in_multicast:port to HLS playlist 
 *   
 *   @param in_multicast : multicast of input stream
 *   @param port: output multicast port numper 
 *   @param hls_root: Path of HLS playlist
 *
 * */
void gst_convert_udp_to_hls(
        const string in_multicast, 
        int port, 
        const string hls_root, 
        const string chan_name)
{

    const string in_url = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start " << in_url << " -> " << hls_root;
    Hdata h {
        .added_audio = false,
        .added_video = false
    };
    strncpy(h.chan_name, chan_name.c_str(),200);
    h.d.loop      = g_main_loop_new(nullptr, false);
    h.d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    try{
        auto udpsrc     = Gst::add_element(h.d.pipeline, "udpsrc"),
             queue_src  = Gst::add_element(h.d.pipeline, "queue", "queue_src"),
             tsdemux    = Gst::add_element(h.d.pipeline, "tsdemux"),
             hlssink    = Gst::add_element(h.d.pipeline, "hlssink2", "hlssink");

        gst_element_link_many(udpsrc, queue_src, tsdemux, nullptr);
        //queue_guard(d.loop, queue_src, 20);

        g_signal_connect(tsdemux, "pad-added", G_CALLBACK(tsdemux_pad_added_h), &h);
        g_object_set(udpsrc, "uri", in_url.c_str(), nullptr);
        const string segment_location = hls_root + "/s_%d.ts";
        const string playlist_location = hls_root + "/p.m3u8";
        g_object_set(hlssink, 
                "max-files", 8,
                "playlist-length", 5,
                "playlist-location", playlist_location.c_str(),
                "location", segment_location.c_str(),
                "message-forward", true,
                "target-duration", 10,
                nullptr);
        // Thread to quit if playlist not update
        std::thread tr([&](){
                while(true){
                    std::this_thread::sleep_for(std::chrono::seconds(60)); 
                    struct stat statbuf;
                    if(!stat(playlist_location.c_str(), &statbuf)){
                        if(statbuf.st_mtim.tv_sec < (time(nullptr)-360) ){
                            LOG(error) << "Playlist is old: " << statbuf.st_mtim.tv_sec
                                << ". quit from HLS of :" << h.chan_name;
                            break;
                            }
                    }else{
                            LOG(error) << "Playlist not found for:" << h.chan_name;
                            break;
                    }
                }
                g_main_loop_quit(h.d.loop);
                });
        Gst::add_bus_watch(h.d);
        gst_element_set_state(GST_ELEMENT(h.d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(h.d.loop);
        tr.join();
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
}
void tsdemux_pad_added_h(GstElement* /*object*/, GstPad* pad, gpointer data)
{
    auto h = (Hdata*) data;
    auto caps = gst_pad_query_caps(pad, nullptr);
    auto caps_struct = gst_caps_get_structure(caps, 0);
    auto pad_type = string(gst_structure_get_name(caps_struct));
    string caps_str = Gst::caps_string(caps);

    GstElement* audiodecoder = nullptr;
    LOG(debug) << Gst::pad_name(pad) << " Caps:" << caps_str;
    GstElement* videoparse = nullptr;
    GstElement* audioparse = nullptr;
    if(pad_type.find("video") != string::npos){
        if(h->added_video){
            LOG(warning) << "Ignore Video, before added video!";
            return;
        }
        h->added_video = true;
    }
    if(pad_type.find("audio") != string::npos){
        if(h->added_audio){
            LOG(warning) << "Ignore Audio, before added audio!";
            return;
        }
        h->added_audio = true;
    }
    if(pad_type.find("video/x-h264") != string::npos){
        videoparse = Gst::add_element(h->d.pipeline, "h264parse", "", true);
        g_object_set(videoparse, "config-interval", 1, nullptr);
    }else if(pad_type.find("video/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        LOG(debug) << "Mpeg version type:" <<  m_version;
        if(m_version == 4){
            videoparse = Gst::add_element(h->d.pipeline, "mpeg4videoparse", "", true);
            g_object_set(videoparse, "config-interval", 1, nullptr);
        }else{
            videoparse = Gst::add_element(h->d.pipeline, "mpegvideoparse", "", true);
        }
    }else if(pad_type.find("audio/mpeg") != string::npos){
        int m_version = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &m_version);
        LOG(debug) << "Mpeg version type:" <<  m_version;
        if(m_version == 1){
            audioparse = Gst::add_element(h->d.pipeline, "mpegaudioparse", "", true);
        }else{
            if(caps_str.find("loas") != string::npos){
                LOG(debug) << "Add LATM audio transcoder";
                // convert audio codec, becuase hlssink not accept aac/loas format
                audiodecoder = Gst::add_element(h->d.pipeline, "aacparse", "", true);
                auto decoder = Gst::add_element(h->d.pipeline, "avdec_aac_latm", 
                        "", true);
                auto audioconvert = Gst::add_element(h->d.pipeline, "audioconvert", "", true);
                auto queue = Gst::add_element(h->d.pipeline, "queue", "", true);
                auto lamemp3enc = Gst::add_element(h->d.pipeline, "lamemp3enc", "", true);
                audioparse = Gst::add_element(h->d.pipeline, "mpegaudioparse", 
                        "", true);
                gst_element_link_many(audiodecoder, decoder, audioconvert, queue, 
                        lamemp3enc, audioparse, nullptr);
            }else{
                audioparse = Gst::add_element(h->d.pipeline, "aacparse", "", true);
            }
        }
    }else if(pad_type.find("audio/x-ac3") != string::npos ||
            pad_type.find("audio/ac3") != string::npos){
        audioparse = Gst::add_element(h->d.pipeline, "ac3parse", "", true);
    }else{
        LOG(warning) << "Not support:" << pad_type;
    }
    gst_caps_unref(caps);
    // link tsdemux ---> queue --- > parse -->  qtmux
    auto parse = (videoparse != nullptr) ? videoparse : audioparse;
    auto parse_name = Gst::element_name(parse);
    if(parse != nullptr){
        g_object_set(parse, "disable-passthrough", true, nullptr);
        auto queue = Gst::add_element(h->d.pipeline, "queue", "", true);
        queue_guard(h, queue, 30);
        if(!Gst::pad_link_element_static(pad, queue, "sink")){
            LOG(error) << "Can't link typefind to queue";
            g_main_loop_quit(h->d.loop); return; 
        }

        if(audiodecoder){
            gst_element_link(queue, audiodecoder);
        }else{
            gst_element_link(queue, parse);
        }
        auto hlssink = gst_bin_get_by_name(GST_BIN(h->d.pipeline), "hlssink");
        string type = videoparse ? "video" : "audio";
        if(!Gst::element_link_request(parse, "src", hlssink, type.c_str())){
            LOG(warning) << "Error to link to hls  " << type << " in " << h->chan_name;
        }
        gst_object_unref(hlssink);
    }
}
void queue_overrun(GstElement* queue, gpointer user_data)
{
    Hdata* h = (Hdata*) user_data;
    long current_level_bytes;
    g_object_get(queue, "current-level-bytes", &current_level_bytes, nullptr);
    LOG(error) << "Exit form channel '" << h->chan_name
        << "' Queue overrun by MB:" << current_level_bytes/1000'000L;
    g_main_loop_quit(h->d.loop);
}
void queue_guard(Hdata* h, GstElement* queue, int sec)
{
    g_object_set(queue,
            "max-size-buffers", 0,
            "max-size-bytes", 0,
            "max-size-time", sec * 1000'000'000L, // in ns
            nullptr);
    g_signal_connect(queue, "overrun", G_CALLBACK( queue_overrun ), h);
}
/*
 *   2020-09-14 17:02:18.383436 iptv_out_hls 0x00007faf6a7fc700 error: [element_link_request:211] Can't get request sink pad from hlssink by name audio

 * */
