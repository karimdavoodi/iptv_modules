#include "gst.hpp"
#include <iostream>
#include <thread>
#include <sstream>
using namespace std;

namespace Gst {
    void init()
    {
        gst_init(NULL, NULL);
    }
    void print_int_property_delay(GstElement* element, const char* attr, int seconds)
    {
        std::thread t([&](){
                    std::this_thread::sleep_for(std::chrono::seconds(seconds));
                    if(GST_OBJECT_REFCOUNT_VALUE(element) == 0 ) return;
                    guint d;
                    g_object_get(element, attr, &d, NULL);
                    BOOST_LOG_TRIVIAL(info) << "Read  " << attr << ":" << d; 
                    });
            t.detach();
    }
    void zero_queue_buffer(GstElement* queue)
    {
        g_object_set(queue,
                "max-size-buffers", 0,
                "max-size-bytes", 0,
                "max-size-time", 0,
                NULL);
    }
    const std::string pad_caps_type(GstPad* pad)
    {
        auto caps_filter = gst_caps_new_any();
        auto caps = gst_pad_query_caps(pad, caps_filter);
        auto caps_struct = gst_caps_get_structure(caps, 0);
        auto pad_type = string(gst_structure_get_name(caps_struct));
        gst_caps_unref(caps_filter);
        gst_caps_unref(caps);
        return pad_type;
    }
    const std::string pad_caps_string(GstPad* pad)
    {
        auto caps_filter = gst_caps_new_any();
        auto caps = gst_pad_query_caps(pad, caps_filter);
        std::string caps_string = gst_caps_to_string(caps);
        gst_caps_unref(caps_filter);
        gst_caps_unref(caps);
        return caps_string;
    }
    GstElement* add_element(GstPipeline* pipeline, const std::string plugin, 
                                                  const std::string name, bool stat_playing)
    {
        GstElement* element = NULL;
        if(name == "") element = gst_element_factory_make(plugin.c_str(), NULL);
        else           element = gst_element_factory_make(plugin.c_str(), name.c_str());
        if(element == NULL){
            BOOST_LOG_TRIVIAL(error) << "Can't make elemen " << name;
            return NULL;
        }
        gst_bin_add(GST_BIN(pipeline), element);
        if(stat_playing) 
            gst_element_set_state(element, GST_STATE_PLAYING);
        BOOST_LOG_TRIVIAL(info) << "Make element " << gst_element_get_name(element);
        return element;
    }
    void dot_file(const GstPipeline* pipeline, const std::string name, int sec)
    {
        char* env = getenv("GST_DEBUG_DUMP_DOT_DIR");
        string make_dot_file = (env != NULL) ? env : "";
        if(make_dot_file.size()){
            stringstream f_name;
            f_name << name << "_" << std::this_thread::get_id();
            string fname = f_name.str();
            std::thread t([pipeline, fname, sec, make_dot_file](){

                    std::this_thread::sleep_for(std::chrono::seconds(sec));
                    if(GST_OBJECT_REFCOUNT_VALUE(pipeline) == 0 ) return;
                    BOOST_LOG_TRIVIAL(debug) << "Make DOT file " 
                            << make_dot_file << "/" << fname << ".dot";
                    gst_debug_bin_to_dot_file(
                            GST_BIN(pipeline), 
                            GST_DEBUG_GRAPH_SHOW_ALL, 
                            fname.c_str());
                    });
            t.detach();
        }else{
            BOOST_LOG_TRIVIAL(debug) << "Not make DOT file";
        }
    }
        
    bool pad_link_element_static(GstPad* pad, GstElement* element, const string pad_name)
    {
        auto element_pad = gst_element_get_static_pad(element, pad_name.c_str());
        bool ret = gst_pad_link(pad, element_pad);
        gst_object_unref(element_pad);
        if(ret != GST_PAD_LINK_OK){
            BOOST_LOG_TRIVIAL(error) << "Can'n link " << gst_pad_get_name(pad) 
                << " to " << gst_element_get_name(element);
            return false;
        }
        return true;
    }
    bool element_link_request(GstElement* src, const char* src_name, 
            GstElement* sink, const char* sink_name)
    {
        if(!src || !sink){
            BOOST_LOG_TRIVIAL(error) << "invalid src or sink in " << __func__;
            return false;
        }
        auto pad_sink = gst_element_get_request_pad(sink, sink_name);
        if(!pad_sink){
            BOOST_LOG_TRIVIAL(error) << "Can't get pads " << sink_name << " in " << __func__;
            return false;
        }
        auto pad_src  = gst_element_get_static_pad(src, src_name);
        if(!pad_src){
            BOOST_LOG_TRIVIAL(error) << "Can't get pads " << src_name << " in " << __func__;
            return false;
        }
        if(gst_pad_link(pad_src, pad_sink) != GST_PAD_LINK_OK){
            gst_object_unref(pad_sink);
            gst_object_unref(pad_src);
            BOOST_LOG_TRIVIAL(error) << "Can't link pads in " <<  __func__;
            return false;
        }
        gst_object_unref(pad_sink);
        gst_object_unref(pad_src);
        return true;
    }
    int on_bus_message (GstBus * bus, GstMessage * message, gpointer user_data)
        {
            GMainLoop* loop = (GMainLoop*)user_data;
            BOOST_LOG_TRIVIAL(trace) 
                << "Message from:" << GST_MESSAGE_SRC_NAME(message)
                << " Type:" << GST_MESSAGE_TYPE_NAME(message);
            
            switch (GST_MESSAGE_TYPE (message)) {
                case GST_MESSAGE_ERROR:
                    {
                        gchar *debug;
                        GError *err;
                        gst_message_parse_error (message, &err, &debug);
                        BOOST_LOG_TRIVIAL(error) <<  err->message
                            << " debug:" << debug;
                        g_error_free (err);
                        g_free (debug);
                        g_main_loop_quit (loop);
                        break;
                    }
                case GST_MESSAGE_WARNING:
                    {
                        gchar *debug;
                        GError *err;
                        gst_message_parse_warning (message, &err, &debug);
                        BOOST_LOG_TRIVIAL(warning) <<  err->message
                            << " debug:" << debug;
                        g_error_free (err);
                        g_free (debug);
                        break;
                    }
                case GST_MESSAGE_EOS:
                    {
                        BOOST_LOG_TRIVIAL(info) << "EOS";
                        g_main_loop_quit (loop);
                    }
                default:
                    break;
            }
            return true;
        }
    bool add_bus_watch(Data& d){
        if(!d.pipeline){
            BOOST_LOG_TRIVIAL(error) << "Pipeline is NULL";
            return false;
        }
        d.bus = gst_element_get_bus (GST_ELEMENT(d.pipeline));
        if(!d.bus){
            BOOST_LOG_TRIVIAL(error) << "Can't get bus";
            return false;
        }
        d.watch_id = gst_bus_add_watch(d.bus, on_bus_message, d.loop);
        return d.watch_id != 0;
    }

}
