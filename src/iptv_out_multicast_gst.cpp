#include <exception>
#include <thread>
#include <gstreamermm.h>
#include <glibmm.h>
#include <boost/log/trivial.hpp>
#include "config.hpp"

using namespace std;

void gst_task(string  in_multicast, string out_multicast)
{
    using Glib::RefPtr;
    RefPtr<Glib::MainLoop> loop;
    RefPtr<Gst::Pipeline>  pipeline;
    RefPtr<Gst::Element>   udpsrc;
    RefPtr<Gst::Element>   rndbuffersize;
    RefPtr<Gst::Element>   udpsink;
    sigc::connection m_timeout_connection;
    
    try{
        in_multicast += ":" + to_string(INPUT_PORT);
        BOOST_LOG_TRIVIAL(trace) 
            << "Start " 
            << in_multicast 
            << " --> " 
            << out_multicast << ":" << INPUT_PORT;

        loop = Glib::MainLoop::create();
        pipeline = Gst::Pipeline::create();
        
        udpsrc = Gst::ElementFactory::create_element("udpsrc");
        rndbuffersize = Gst::ElementFactory::create_element("rndbuffersize");
        udpsink = Gst::ElementFactory::create_element("udpsink");
        
        if( !udpsrc || !udpsink || !rndbuffersize){
            BOOST_LOG_TRIVIAL(trace) << "Error in create";
            return;
        }
        pipeline->add(udpsrc)->add(rndbuffersize)->add(udpsink);
        udpsrc->link(rndbuffersize);
        rndbuffersize->link(udpsink);
        
        udpsrc->set_property("uri", "udp://"+in_multicast);

        rndbuffersize->set_property("min", 1316);
        rndbuffersize->set_property("max", 1316);

        udpsink->set_property("multicast-iface", string("lo"));
        udpsink->set_property("host", out_multicast);
        udpsink->set_property("port", INPUT_PORT);
        udpsink->set_property("sync", 1);

        pipeline->get_bus()->add_watch([loop](const RefPtr<Gst::Bus>&, 
                                             const RefPtr<Gst::Message>& msg){
               //BOOST_LOG_TRIVIAL(trace) << msg->get_message_type();
                switch(msg->get_message_type()){
                    case Gst::MESSAGE_ERROR:
                        BOOST_LOG_TRIVIAL(warning) << 
                                RefPtr<Gst::MessageError>::cast_static(msg)->parse_debug();
                        break;
                    case Gst::MESSAGE_EOS:
                        loop->quit();
                        break;
                    default: break;
                }
                return true;
                });
        m_timeout_connection = Glib::signal_timeout().connect([&]()->bool {
                gint64 cur;
                pipeline->query_position(Gst::FORMAT_TIME, cur); 
                BOOST_LOG_TRIVIAL(trace) <<  "Position:" << cur/1000000;    
                return true;
                }, TIME_INTERVAL_STATE_SAVE);
        pipeline->set_state(Gst::STATE_PLAYING);
        loop->run();
        pipeline->set_state(Gst::STATE_NULL);
        m_timeout_connection.disconnect();
        BOOST_LOG_TRIVIAL(trace) << "Finish";

    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(trace) << "Exception:" << e.what();
    }
}
