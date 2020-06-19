#include "gst.hpp"
#include <iostream>
#include <thread>
#include <sstream>
#include <boost/log/trivial.hpp>
using namespace std;

namespace Gst {
    void init()
    {
        gst_init(NULL, NULL);
    }
    GstElement* add_element(GstElement* pipeline, const std::string plugin, 
                                                  const std::string name)
    {
        string element_name = (name != "") ? name : plugin;
        GstElement* element = gst_element_factory_make(plugin.c_str(), element_name.c_str());
        if(element == NULL){
            throw std::runtime_error("Can't make elemen" + element_name);
        }
        gst_bin_add(GST_BIN(pipeline), element);
        return element;
    }
    void dot_file(const GstElement* pipeline, const std::string name, int sec)
    {
        char* env = getenv("GST_DEBUG_DUMP_DOT_DIR");
        string make_dot_file = (env != NULL) ? env : "";
        if(make_dot_file.size()){
            stringstream f_name;
            f_name << name << "_" << std::this_thread::get_id();
            string fname = f_name.str();
            std::thread t([pipeline, fname, sec, make_dot_file](){

                    std::this_thread::sleep_for(std::chrono::seconds(sec));
                    BOOST_LOG_TRIVIAL(trace) << "Make DOT file " 
                            << make_dot_file << "/" << fname << ".dot";
                    gst_debug_bin_to_dot_file(
                            GST_BIN(pipeline), 
                            GST_DEBUG_GRAPH_SHOW_ALL, 
                            fname.c_str());
                    });
            t.detach();
        }else{
            BOOST_LOG_TRIVIAL(trace) << "Not make DOT file";
        }
    }
        
    bool pad_link_element_static(GstPad* pad, GstElement* element, const string pad_name)
    {
        auto element_pad = gst_element_get_static_pad(element, pad_name.c_str());
        bool ret = gst_pad_link(pad, element_pad);
        gst_object_unref(element_pad);
        return ret == GST_PAD_LINK_OK;
    }
    bool element_link_request(GstElement* src, const char* src_name, 
            GstElement* sink, const char* sink_name)
    {
        auto pad_sink = gst_element_get_request_pad(sink, sink_name);
        auto pad_src  = gst_element_get_static_pad(src, src_name);
        if(!pad_sink || !pad_src){
            throw std::runtime_error("Can't get pads in " + string( __func__));
        }
        if(gst_pad_link(pad_src, pad_sink) != GST_PAD_LINK_OK){
            gst_object_unref(pad_sink);
            gst_object_unref(pad_src);
            throw std::runtime_error("Can't link pads in " + string( __func__));
        }
        gst_object_unref(pad_sink);
        gst_object_unref(pad_src);
        return true;

    }
    int on_bus_message (GstBus * bus, GstMessage * message, gpointer user_data)
        {
            GMainLoop* loop = (GMainLoop*)user_data;
            //BOOST_LOG_TRIVIAL(trace) 
            //    << "Got message from:" << GST_MESSAGE_SRC_NAME(message)
            //    << " Type:" << GST_MESSAGE_TYPE_NAME(message);
            
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
    guint add_bus_watch(GstElement* pipeline, GMainLoop* loop){
        auto bus = gst_element_get_bus (pipeline);
        auto watch_id = gst_bus_add_watch(bus, on_bus_message, loop);
        gst_object_unref (bus);
        return watch_id;
    }

}
