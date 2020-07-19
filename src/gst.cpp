#include "gst.hpp"
#include <iostream>
#include <thread>
#include <sstream>
#include <cassert>
#define  CHECK_NULL(data, name) if(!data){ LOG(error) << name << ": is NULL!"; return;}
#define  CHECK_NULL_RET(data, name, ret) if(!data){ LOG(error) << name << ": is NULL!"; return ret;}
using namespace std;

namespace Gst {
    void init()
    {
        gst_init(nullptr, nullptr);
    }
    void print_int_property_delay(GstElement* element, const char* attr, int seconds)
    {
        CHECK_NULL(element, "Element");

        std::thread t([&](){
                    std::this_thread::sleep_for(std::chrono::seconds(seconds));
                    if(GST_OBJECT_REFCOUNT_VALUE(element) == 0 ) return;
                    guint d;
                    g_object_get(element, attr, &d, nullptr);
                    LOG(trace) << "Read  " << attr << ":" << d; 
                    });
            t.detach();
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
    const std::string pad_caps_type(GstPad* pad)
    {
        CHECK_NULL_RET(pad, "Pad", "");
        auto caps = gst_pad_query_caps(pad, nullptr);
        auto caps_struct = gst_caps_get_structure(caps, 0);
        auto pad_type = string(gst_structure_get_name(caps_struct));
        gst_caps_unref(caps);
        return pad_type;
    }
    const std::string element_name(GstElement* element)
    {
        CHECK_NULL_RET(element, "Element", "");
        if(!element) return "";
        auto name = gst_element_get_name(element);
        std::string result {name};
        g_free(name);
        return result;
    }
    const std::string pad_name(GstPad* pad)
    {
        CHECK_NULL_RET(pad, "Pad", "");
        auto name = gst_pad_get_name(pad);
        std::string result {name};
        g_free(name);
        return result;
    }
    const std::string caps_string(GstCaps* caps)
    {
        CHECK_NULL_RET(caps, "Caps", "");
        auto name = gst_caps_to_string(caps);
        std::string result {name};
        g_free(name);
        return result;
    }
    const std::string pad_caps_string(GstPad* pad)
    {
        CHECK_NULL_RET(pad, "Pad", "");
        auto caps = gst_pad_query_caps(pad, nullptr);
        auto caps_string = gst_caps_to_string(caps);
        string result{caps_string};

        gst_caps_unref(caps);
        g_free(caps_string);

        return result;
    }
    GstElement* add_element(GstPipeline* pipeline, const std::string plugin, 
                                                  const std::string name, bool stat_playing)
    {
        CHECK_NULL_RET(pipeline, "Pipeline", nullptr);
        GstElement* element = nullptr;
        if(name == "") element = gst_element_factory_make(plugin.c_str(), nullptr);
        else           element = gst_element_factory_make(plugin.c_str(), name.c_str());
        if(!element) LOG(error) << "Element not found:" << plugin;
        CHECK_NULL_RET(element, "Can't make element", nullptr);

        gst_bin_add(GST_BIN(pipeline), element);

        if(stat_playing) 
            gst_element_set_state(element, GST_STATE_PLAYING);

        LOG(trace) << "Make element " << element_name(element);
        
        return element;
    }
    void pipeline_timeout(Data& d, int sec)
    {
        CHECK_NULL(d.pipeline, "Pipeline");
        std::thread t([&](){
                    std::this_thread::sleep_for(std::chrono::seconds(sec));
                    if(GST_OBJECT_REFCOUNT_VALUE(d.pipeline) == 0 ) return;
                    LOG(trace) << "Pipeline timeout reached"; 
                    if(d.loop) g_main_loop_quit(d.loop);
                    });
        t.detach();
    }
    void dot_file(const GstPipeline* pipeline, const std::string name, int sec)
    {
        CHECK_NULL(pipeline, "Pipeline");
        char* env = getenv("GST_DEBUG_DUMP_DOT_DIR");
        string make_dot_file = (env != nullptr) ? env : "";
        if(make_dot_file.size()){
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
        
    bool pad_link_element_request(GstPad* pad, GstElement* element, const string e_pad_name)
    {
        CHECK_NULL_RET(pad, "Pad", false);
        CHECK_NULL_RET(element, "Element", false);

        auto element_pad = gst_element_get_request_pad(element, e_pad_name.c_str());
        CHECK_NULL_RET(element_pad, "Can't get request pad", false);
        
        bool ret = gst_pad_link(pad, element_pad);
        if(ret != GST_PAD_LINK_OK){
            LOG(trace) << "Can't link " << pad_name(pad) << " to " << element_name(element) 
                << ":" << pad_name(element_pad);
            gst_object_unref(element_pad);
            return false;
        }else{
            LOG(trace) << "Link " << pad_name(pad) << " to " << element_name(element) 
                << ":" << pad_name(element_pad);
            gst_object_unref(element_pad);
            return true;
        }
    }
    bool pad_link_element_static(GstPad* pad, GstElement* element, const string e_pad_name)
    {
        CHECK_NULL_RET(pad, "Pad", false);
        CHECK_NULL_RET(element, "Element", false);

        auto element_pad = gst_element_get_static_pad(element, e_pad_name.c_str());
        CHECK_NULL_RET(element_pad, "Can't get static pad", false);

        bool ret = gst_pad_link(pad, element_pad);
        gst_object_unref(element_pad);
        if(ret != GST_PAD_LINK_OK){
            LOG(error) << "Can'n link " << pad_name(pad) << " to " << element_name(element);
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
        CHECK_NULL_RET(pad_sink, "Can't get sink request pad", false);

        auto pad_src  = gst_element_get_static_pad(src, src_name);
        CHECK_NULL_RET(pad_src, "Can't get request pad", false);

        if(gst_pad_link(pad_src, pad_sink) != GST_PAD_LINK_OK){
            LOG(error) << "Can'n link " << 
                element_name(src) << ":" << src_name << " to " << 
                element_name(sink) << ":" << sink_name; 
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
    int on_bus_message (GstBus * bus, GstMessage * message, gpointer user_data)
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

}
