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
#include "gst.hpp"
#include <iostream>
#include <thread>
#include <sstream>
#define  CHECK_NULL(data, name) \
            do{ \
               if(!(data)){ LOG(error) << (name) << ": is NULL!"; return;} \
            }while(0)
#define  CHECK_NULL_RET(data, name, ret) \
            do{ \
               if(!(data)){ LOG(error) << (name) << ": is NULL!"; return ret;} \
            }while(0)

using namespace std;

namespace Gst {
    void init()
    {
        gst_init(nullptr, nullptr);
    }
    void zero_queue_buffer(GstElement* queue)
    {
        CHECK_NULL(queue, "Queue");
        g_object_set(queue,
                "max-size-buffers", 0,
                "max-size-bytes", 0,
                "max-size-time", 0,
                //"leaky", 2,
                nullptr);
    }
    std::string pad_caps_type(GstPad* pad)
    {
        CHECK_NULL_RET(pad, "Pad", "");
        auto caps = gst_pad_query_caps(pad, nullptr);
        auto caps_struct = gst_caps_get_structure(caps, 0);
        auto pad_type = string(gst_structure_get_name(caps_struct));
        gst_caps_unref(caps);
        return pad_type;
    }
    std::string element_name(GstElement* element)
    {
        CHECK_NULL_RET(element, "Element", "");
        auto name = gst_element_get_name(element);
        std::string result {name};
        g_free(name);
        return result;
    }
    std::string pad_name(GstPad* pad)
    {
        CHECK_NULL_RET(pad, "Pad", "");
        auto name = gst_pad_get_name(pad);
        std::string result {name};
        g_free(name);
        return result;
    }
    std::string caps_string(GstCaps* caps)
    {
        CHECK_NULL_RET(caps, "Caps", "");
        auto name = gst_caps_to_string(caps);
        std::string result {name};
        g_free(name);
        return result;
    }
    std::string pad_caps_string(GstPad* pad)
    {
        CHECK_NULL_RET(pad, "Pad", "");
        auto caps = gst_pad_query_caps(pad, nullptr);
        auto caps_string = gst_caps_to_string(caps);
        string result{caps_string};

        gst_caps_unref(caps);
        g_free(caps_string);

        return result;
    }
    GstElement* add_element(GstPipeline* pipeline, const std::string& plugin,
                                                  const std::string& name, bool stat_playing)
    {
        CHECK_NULL_RET(pipeline, "Pipeline", nullptr);
        GstElement* element;
        if(name.empty()) element = gst_element_factory_make(plugin.c_str(), nullptr);
        else             element = gst_element_factory_make(plugin.c_str(), name.c_str());
        if(!element) LOG(error) << "Element not found:" << plugin;
        CHECK_NULL_RET(element, "Can't make element", nullptr);

        gst_bin_add(GST_BIN(pipeline), element);

        if(stat_playing) 
            gst_element_set_state(element, GST_STATE_PLAYING);

        LOG(trace) << "Make element " << element_name(element);
        
        return element;
    }

    void dot_file(const GstPipeline* pipeline, const std::string& name, int sec)
    {
        CHECK_NULL(pipeline, "Pipeline");
        char* env = getenv("GST_DEBUG_DUMP_DOT_DIR");
        string make_dot_file = (env != nullptr) ? env : "";
        if(!make_dot_file.empty()){
            stringstream f_name;
            f_name << name << "_" << std::this_thread::get_id();
            string fname = f_name.str();
            std::thread t([pipeline, fname, sec, make_dot_file](){

                    std::this_thread::sleep_for(std::chrono::seconds(sec));
                    if(GST_OBJECT_REFCOUNT_VALUE(pipeline) == 0 ) return;
                    LOG(trace) << "Make DOT file "
                            << make_dot_file << "/" << fname << ".dot";
                    gst_debug_bin_to_dot_file(
                            GST_BIN(pipeline),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            fname.c_str());
                    });
            t.detach();
        }else{
            LOG(trace) << "Not make DOT file";
        }
    }

    bool pad_link_element_request(GstPad* pad, GstElement* element, const string& e_pad_name)
    {
        CHECK_NULL_RET(pad, "Pad", false);
        CHECK_NULL_RET(element, "Element", false);

        auto element_pad = gst_element_get_request_pad(element, e_pad_name.c_str());
        CHECK_NULL_RET(element_pad, "Can't get request pad", false);
        
        bool ret = gst_pad_link(pad, element_pad);
        if(ret != GST_PAD_LINK_OK){
            LOG(error) << "Can't link " << pad_name(pad) 
                << " to " << element_name(element) 
                << ":" << pad_name(element_pad)
                << " SRC PAD CAPS:" << pad_caps_string(pad);
            gst_object_unref(element_pad);
            return false;
        }else{
            LOG(trace) << "Link " << pad_name(pad) << " to " << element_name(element) 
                << ":" << pad_name(element_pad);
            gst_object_unref(element_pad);
            return true;
        }
    }
    bool pad_link_element_static(GstPad* pad, GstElement* element, const string& e_pad_name)
    {
        CHECK_NULL_RET(pad, "Pad", false);
        CHECK_NULL_RET(element, "Element", false);

        auto element_pad = gst_element_get_static_pad(element, e_pad_name.c_str());
        CHECK_NULL_RET(element_pad, "Can't get static pad", false);

        bool ret = gst_pad_link(pad, element_pad);
        gst_object_unref(element_pad);
        if(ret != GST_PAD_LINK_OK){
            LOG(error) << "Can'n link " << pad_name(pad) 
                << " to " << element_name(element)
                << " SRC PAD CAPS:" << pad_caps_string(pad);
            return false;
        }
        LOG(trace) << "Link " << pad_name(pad) << " to " << element_name(element);
        return true;
    }
    bool element_link_request(GstElement* src, const char* src_name, 
            GstElement* sink, const char* sink_name)
    {
        CHECK_NULL_RET(src, "Src Element", false);
        CHECK_NULL_RET(sink, "Sink Element", false);

        auto pad_sink = gst_element_get_request_pad(sink, sink_name);
        if(!pad_sink){
            LOG(error) << "Can't get request sink pad from " 
                << element_name(sink) 
                << " by name " << sink_name;
            return false;
        }

        auto pad_src  = gst_element_get_static_pad(src, src_name);
        if(!pad_src){
            LOG(error) << "Can't get static src pad from " 
                << element_name(src) 
                << " by name " << src_name;
            return false;
        }

        if(gst_pad_link(pad_src, pad_sink) != GST_PAD_LINK_OK){
            LOG(error) << "Can'n link " << 
                element_name(src) << ":" << src_name << " to " << 
                element_name(sink) << ":" << sink_name
                << " SRC PAD CAPS:" << pad_caps_string(pad_src);
            gst_object_unref(pad_sink);
            gst_object_unref(pad_src);
            return false;
        }
        LOG(trace) << "Link " << 
                element_name(src) << ":" << pad_name(pad_src) << " to " << 
                element_name(sink) << ":" << pad_name(pad_sink); 
        gst_object_unref(pad_sink);
        gst_object_unref(pad_src);
        return true;
    }
    int on_bus_message (GstBus * , GstMessage * message, gpointer user_data)
        {
            auto d = (Data*) user_data;
            if(!d->pipeline) return true;
            //if(message->src)
            //    LOG(trace) 
            //    << "Message from:" << GST_MESSAGE_SRC_NAME(message)
            //    << " Type:" << GST_MESSAGE_TYPE_NAME(message);
            
            switch (GST_MESSAGE_TYPE (message)) {
                case GST_MESSAGE_ERROR:
                    {
                        gchar *debug;
                        GError *err;
                        gst_message_parse_error (message, &err, &debug);
                        LOG(error) <<  err->message << " debug:" << debug;
                        g_error_free (err);
                        g_free (debug);
                        char* path = getenv("GST_DEBUG_DUMP_DOT_DIR");
                        if(path != nullptr){
                            stringstream f_name;
                            f_name << "error_" << std::this_thread::get_id();
                            string fname = f_name.str();
                            LOG(info) << "Make DOT file in " << path 
                                << "/" << fname << ".dot";
                            gst_debug_bin_to_dot_file(
                                    GST_BIN(d->pipeline), 
                                    GST_DEBUG_GRAPH_SHOW_ALL, 
                                    fname.c_str() );
                        }
                        if(d->loop) g_main_loop_quit (d->loop);
                        break;
                    }
                case GST_MESSAGE_WARNING:
                    {
                        gchar *debug;
                        GError *err;
                        gst_message_parse_warning (message, &err, &debug);
                        LOG(warning) <<  err->message << " debug:" << debug;
                        g_error_free (err);
                        g_free (debug);
                        break;
                    }
                case GST_MESSAGE_EOS:
                    {
                        LOG(info) << "Pipeline got EOS";
                        if(d->loop) g_main_loop_quit (d->loop);
                    }
                default:
                    break;
            }
            return true;
        }
    bool add_bus_watch(Data& d){
        CHECK_NULL_RET(d.pipeline, "Pipeline", false);

        d.bus = gst_element_get_bus (GST_ELEMENT(d.pipeline));
        CHECK_NULL_RET(d.bus, "Can't get bus", false);

        d.watch_id = gst_bus_add_watch(d.bus, on_bus_message, &d);
        return d.watch_id != 0;
    }
    bool demux_pad_link_to_muxer(
            GstPipeline* pipeline,
            GstPad* pad,
            const std::string_view muxer_element_name,
            const std::string_view muxer_audio_pad_name,
            const std::string_view muxer_video_pad_name,
            bool queue_buffer_zero
            ){
        auto caps = gst_pad_query_caps(pad, gst_caps_new_any());
        auto caps_struct = gst_caps_get_structure(caps, 0);
        auto pad_type = string(gst_structure_get_name(caps_struct));
        auto caps_str = Gst::caps_string(caps);
        
        LOG(debug) << Gst::pad_name(pad) << " Caps:" << caps_str;
        
        GstElement* videoparse   = nullptr;
        GstElement* audioparse   = nullptr;
        GstElement* audiodecoder = nullptr;

        if(pad_type.find("video/x-h264") != string::npos){
            videoparse = Gst::add_element(pipeline, "h264parse", "", true);
            g_object_set(videoparse, "config-interval", 1, nullptr);
        }else if(pad_type.find("video/mpeg") != string::npos){
            int mpegversion = 1;
            gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
            LOG(debug) << "Mpeg version type:" <<  mpegversion;
            if(mpegversion == 4){
                videoparse = Gst::add_element(pipeline, "mpeg4videoparse", "", true);
                g_object_set(videoparse, "config-interval", 1, nullptr);
            }else{
                videoparse = Gst::add_element(pipeline, "mpegvideoparse", "", true);
            }
        }else if(pad_type.find("audio/mpeg") != string::npos){
            int mpegversion = 1;
            gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
            LOG(debug) << "Mpeg version type:" <<  mpegversion;
            if(mpegversion == 1){
                audioparse = Gst::add_element(pipeline, "mpegaudioparse", "", true);
            }else if(mpegversion == 2 || mpegversion == 4){
                if(caps_str.find("loas") != string::npos){
                    LOG(warning) << "Add transcoder for latm";
                    audiodecoder = Gst::add_element(pipeline, "aacparse", "", true);
                    auto decoder = Gst::add_element(pipeline, "avdec_aac_latm", 
                            "", true);
                    auto audioconvert = Gst::add_element(pipeline, "audioconvert", "", true);
                    auto queue = Gst::add_element(pipeline, "queue", "", true);
                    auto lamemp3enc = Gst::add_element(pipeline, "lamemp3enc", "", true);
                    audioparse = Gst::add_element(pipeline, "mpegaudioparse", 
                            "", true);
                    gst_element_link_many(audiodecoder, decoder, audioconvert, queue, 
                            lamemp3enc, audioparse, nullptr);
                    g_object_set(audiodecoder, "disable-passthrough", true, nullptr);
                }else{
                    audioparse = Gst::add_element(pipeline, "aacparse", "", true);
                }
            }
        }else if(pad_type.find("audio/x-ac3") != string::npos ||
                pad_type.find("audio/ac3") != string::npos){
            audioparse = Gst::add_element(pipeline, "ac3parse", "", true);
        }else{
            LOG(error) << "Not support:" << pad_type;
            gst_caps_unref(caps);
            return false;
        }
        gst_caps_unref(caps);
        // link demux ---> queue --- > parse --> muxer
        auto parse = (videoparse != nullptr) ? videoparse : audioparse;
        auto parse_name = element_name(parse);
        if(parse != nullptr){
            g_object_set(parse, "disable-passthrough", true, nullptr);
            auto queue = Gst::add_element(pipeline, "queue", "", true);
            if(queue_buffer_zero){
                Gst::zero_queue_buffer(queue);  // effect on iptv_in_archive memory consume 
            } 
            if(!Gst::pad_link_element_static(pad, queue, "sink")){
                LOG(error) << "Can't link typefind to queue, SRC PAD CAPS:"
                    << Gst::pad_caps_string(pad);
            }
            if(audiodecoder){
                gst_element_link(queue, audiodecoder);
            }else{
                gst_element_link(queue, parse);
            }
            auto mux = gst_bin_get_by_name(GST_BIN(pipeline), muxer_element_name.data());
            if(!mux){
                LOG(error) << "Can't find element in pipeline:" << muxer_element_name;
                return false;
            }
            auto sink_name = (videoparse != nullptr) ? 
                muxer_video_pad_name : 
                muxer_audio_pad_name;
            Gst::element_link_request(parse, "src", mux, sink_name.data());
            gst_object_unref(mux);
            return true;
        }
        return false;
    }
    GstElement* insert_parser(GstPipeline* pipeline, GstPad* pad)
    {
        auto caps = gst_pad_query_caps(pad, nullptr);
        auto caps_struct = gst_caps_get_structure(caps, 0);
        auto pad_type = string(gst_structure_get_name(caps_struct));

        GstElement* element = nullptr;
        if(pad_type.find("video/x-h264") != string::npos){
            element = Gst::add_element(pipeline, "h264parse", "", true);
            g_object_set(element, "config-interval", 1, nullptr);
        }else if(pad_type.find("video/mpeg") != string::npos){
            int m_version = 1;
            gst_structure_get_int(caps_struct, "mpegversion", &m_version);
            LOG(debug) << "Mpeg version type:" <<  m_version;
            if(m_version == 4){
                element = Gst::add_element(pipeline, "mpeg4videoparse", "", true);
                g_object_set(element, "config-interval", 1, nullptr);
            }else{
                element = Gst::add_element(pipeline, "mpegvideoparse", "", true);
            }
        }else if(pad_type.find("audio/mpeg") != string::npos){
            int m_version = 1;
            gst_structure_get_int(caps_struct, "mpegversion", &m_version);
            LOG(debug) << "Mpeg version type:" <<  m_version;
            if(m_version == 1){
                element = Gst::add_element(pipeline, "mpegaudioparse", "", true);
            }else{
                element = Gst::add_element(pipeline, "aacparse", "", true);
            }
        }else if(pad_type.find("audio/x-ac3") != string::npos ||
                pad_type.find("audio/ac3") != string::npos){
            element = Gst::add_element(pipeline, "ac3parse", "", true);
        }else{
            LOG(error) << "Type not support:" << pad_type;
        }
        if(element){
            g_object_set(element, "disable-passthrough", true, nullptr);
        }else{
            LOG(error) << "Not add parser";
        }
        return element;
    }
}

