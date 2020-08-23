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
#include <boost/log/trivial.hpp>
#include "gst.hpp"
using namespace std;

void urisourcebin_pad_added(GstElement* urisourcebin, 
                                    GstPad* pad, gpointer data);
void demux_padd_added_n(GstElement* object, GstPad* pad, gpointer data);
void typefind_have_type_n(GstElement* typefind,
                                     guint arg0,
                                     GstCaps* caps,
                                     gpointer user_data);

/*
 *   The Gstreamer main function
 *   Stream network stream to udp:://out_multicast:port
 *   
 *   @param url: URL of input stream in UDP, HTTP, RTSP, RTP
 *   @param out_multicast : multicast of output stream
 *   @param port: output multicast port numper 
 *
 * */
void gst_convert_stream_to_udp(string url, string out_multicast, int port)
{
    LOG(info) 
        << "Start " << url 
        << " --> udp://" << out_multicast << ":" << port;
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
        g_signal_connect(typefind, "have-type", G_CALLBACK(typefind_have_type_n), &d);
        g_object_set(urisourcebin, "uri", url.c_str(), nullptr);
        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
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
void demux_padd_added_n(GstElement* object, GstPad* pad, gpointer data)
{
    auto d = (Gst::Data*) data;
    Gst::demux_pad_link_to_muxer(d->pipeline, pad, "mpegtsmux", "sink_%d", "sink_%d");
}
void typefind_have_type_n(GstElement* typefind,
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
            g_signal_connect(demux, "pad-added", G_CALLBACK(demux_padd_added_n), d);
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
            g_signal_connect(demux, "pad-added", G_CALLBACK(demux_padd_added_n), d);
        }
    }
}
