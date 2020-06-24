#pragma onc;
#include "config.hpp"
#include <iostream>
#include <gst/gst.h>
namespace Gst {
    struct Data{
        GMainLoop* loop;
        GstElement* pipeline;
    };
    void init();
    void print_int_property_delay(GstElement* element, const char* attr, int seconds);
    void zero_queue_buffer(GstElement* queue);
    const std::string pad_caps_string(GstPad* pad);
    void dot_file(const GstElement* pipeline, const std::string name, int sec = 5);
    GstElement* add_element(GstElement* pipeline, const std::string plugin, 
                            const std::string name = "", bool stat_playing = false);
    bool pad_link_element_static(GstPad* pad, GstElement* element, const std::string pad_name);
    guint add_bus_watch(GstElement* pipeline, GMainLoop* loop);
    bool element_link_request(GstElement* src, const char* src_name, 
            GstElement* sink, const char* sink_name);
    
}
/*
#define PIPLINE_WATCH \
    pipeline->get_bus()->add_watch([loop](const RefPtr<Gst::Bus>&, \
                                     const RefPtr<Gst::Message>& msg){\
        BOOST_LOG_TRIVIAL(trace) \
            << "Got Msg: " << msg->get_message_type()\
            << " From:  " << msg->get_source()->get_name()\
            << " Struct: " <<  msg->get_structure().to_string();\
        switch(msg->get_message_type()){\
            case Gst::MESSAGE_INFO:\
                BOOST_LOG_TRIVIAL(info) << \
                        RefPtr<Gst::MessageInfo>::cast_static(msg)->parse_debug();\
                break;\
            case Gst::MESSAGE_WARNING:\
                BOOST_LOG_TRIVIAL(warning) << \
                        RefPtr<Gst::MessageWarning>::cast_static(msg)->parse_debug();\
                break;\
            case Gst::MESSAGE_ERROR:\
                BOOST_LOG_TRIVIAL(error) << \
                        RefPtr<Gst::MessageError>::cast_static(msg)->parse_debug();\
                break;\
            case Gst::MESSAGE_EOS:\
                BOOST_LOG_TRIVIAL(info) <<  "Got EOS";    \
                loop->quit();\
                break;\
            default: break;\
        }\
        return true;\
    });
#define PIPLINE_POSITION \
    sigc::connection m_timeout_pos = Glib::signal_timeout().connect([&]()->bool {\
        gint64 cur;\
        pipeline->query_position(Gst::FORMAT_TIME, cur); \
        BOOST_LOG_TRIVIAL(trace) <<  "Position:" << cur/1000000;    \
        return true;\
    }, TIME_INTERVAL_STATE_SAVE )
#define PIPLINE_DOT_FILE \
    sigc::connection m_timeout_dot = Glib::signal_timeout().connect([&]()->bool {\
            BOOST_LOG_TRIVIAL(trace) << "Make dot file: pipeline.dot";\
            GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline->gobj()), \
                    GST_DEBUG_GRAPH_SHOW_ALL, "pipeline");\
                    return false;\
    }, TIME_INTERVAL_STATE_SAVE )
*/
