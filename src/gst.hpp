#pragma onc;
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

        Data():loop(NULL), pipeline(NULL), bus(NULL), watch_id(0){}
        ~Data(){
            if(pipeline != NULL){
                gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
                gst_object_unref(pipeline);
            }
            if(bus != NULL){
                gst_object_unref(bus);
            }
            if(watch_id){
                g_source_remove(watch_id);
            } 
            if(loop != NULL){
                g_main_loop_unref(loop);
            }
            bus = NULL; pipeline = NULL; loop = NULL; watch_id = 0;
            BOOST_LOG_TRIVIAL(debug) << "Finish";
        }
    };
    void init();
    void pipeline_timeout(Data& d, int sec);
    void print_int_property_delay(GstElement* element, const char* attr, int seconds);
    void zero_queue_buffer(GstElement* queue);
    const std::string pad_caps_string(GstPad* pad);
    const std::string pad_caps_type(GstPad* pad);
    void dot_file(const GstPipeline* pipeline, const std::string name, int sec = 5);
    GstElement* add_element(GstPipeline* pipeline, const std::string plugin, 
            const std::string name = "", bool stat_playing = false);
    bool pad_link_element_static(GstPad* pad, GstElement* element, const std::string pad_name);
    bool add_bus_watch(Data& d);
    bool element_link_request(GstElement* src, const char* src_name, 
            GstElement* sink, const char* sink_name);

}
