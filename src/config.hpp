#pragma onc;
#define PKT_SIZE     1316
#define SOCK_BUF_SIZE  (PKT_SIZE*4000)
#define  INPUT_PORT 3200
#define  MEDIA_ROOT "/opt/sms/www/iptv/media/"
#define  HLS_ROOT "/opt/sms/tmp/HLS/"
#define  FFMPEG "/usr/bin/ffmpeg -v quiet "
#define INPUT_MULTICAST 229
#define TIME_INTERVAL_STATE_SAVE 3000
#define EPG_UPDATE_TIME  15*60*1000   // every 15 minute
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
