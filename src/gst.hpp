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
#pragma once
#include "config.hpp"
#include <iostream>
#include <gst/gst.h>
#include <boost/log/trivial.hpp>
namespace Gst {
    struct Data{
        GMainLoop*      loop;
        GstPipeline*    pipeline;
        GstBus*         bus;
        guint           watch_id;

        Data():loop(nullptr), pipeline(nullptr), bus(nullptr), watch_id(0){}
        ~Data(){
            try{
                if(pipeline != nullptr){
                    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
                    gst_object_unref(pipeline);
                }
                if(bus != nullptr){
                    gst_object_unref(bus);
                }
                if(watch_id){
                    g_source_remove(watch_id);
                } 
                if(loop != nullptr){
                    g_main_loop_unref(loop);
                }
                bus = nullptr; pipeline = nullptr; loop = nullptr; watch_id = 0;
                BOOST_LOG_TRIVIAL(info) << "Clean pipeline";
            }catch(std::exception& e){
                LOG(error) << e.what();
            }
        }
    };
    void init();
    const std::string element_name(GstElement* element);
    const std::string pad_name(GstPad* element);
    const std::string caps_string(GstCaps* caps);
    void pipeline_timeout(Data& d, int sec);
    void print_int_property_delay(GstElement* element, const char* attr, int seconds);
    void zero_queue_buffer(GstElement* queue);
    const std::string pad_caps_string(GstPad* pad);
    const std::string pad_caps_type(GstPad* pad);
    void dot_file(const GstPipeline* pipeline, const std::string name, int sec = 5);
    GstElement* add_element(GstPipeline* pipeline, const std::string plugin, 
            const std::string name = "", bool stat_playing = false);
    bool pad_link_element_static(GstPad* pad, GstElement* element, const std::string pad_name);
    bool pad_link_element_request(GstPad* pad, GstElement* element, const std::string pad_name);
    bool add_bus_watch(Data& d);
    bool element_link_request(GstElement* src, const char* src_name, 
              GstElement* sink, const char* sink_name);
    GstElement* insert_parser(GstPipeline* pipeline, GstPad* pad);
    bool demux_pad_link_to_muxer(
            GstPipeline* pipeline,
            GstPad* pad,
            const std::string_view muxer_element_name,
            const std::string_view muxer_audio_pad_name,
            const std::string_view muxer_video_pad_name,
            bool queue_buffer_zero = true);

}
