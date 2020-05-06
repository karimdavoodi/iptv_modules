#include "gstreamermm/element.h"
#include "gstreamermm/pipeline.h"
#include <exception>
#include <boost/log/trivial.hpp>
#include <gstreamermm.h>
#include <glibmm.h>
#include <thread>
#include <iostream>
#define TIME_INTERVAL_STATE_SAVE 3000  // msec
/*
 
            bin ---> rndbuffersize ---> udpsink

 * */
using namespace std;
using Glib::RefPtr;
bool make_src_bin(string url, 
        RefPtr<Gst::Pipeline>& pipeline,
        RefPtr<Gst::Element>& next_element)
{
    if(url.find("udp://") != string::npos || url.find("rtp://") != string::npos){
        RefPtr<Gst::Element>   udpsrc;
        udpsrc = Gst::ElementFactory::create_element("udpsrc");
        udpsrc->set_property("uri", url);
        pipeline->add(udpsrc);
        udpsrc->link(next_element);
    }else if(url.find("rtsp://") != string::npos){
        RefPtr<Gst::Element>   rtspsrc;
        rtspsrc = Gst::ElementFactory::create_element("rtspsrc");
        rtspsrc->set_property("location", url);
        pipeline->add(rtspsrc);
        rtspsrc->signal_pad_added().connect(
                [&] (const Glib::RefPtr<Gst::Pad>& pad) {
                std::cout << "Pad name: " << pad->get_name() << std::endl;
                pad->link(next_element->get_static_pad("sink"));
                });
    }else if(url.find("http://") != string::npos ||
             url.find("https://") != string::npos){
        RefPtr<Gst::Element>   souphttpsrc;
        souphttpsrc = Gst::ElementFactory::create_element("souphttpsrc");
        souphttpsrc->set_property("location", url);
        souphttpsrc->set_property("is-live", 1);
        pipeline->add(souphttpsrc);
        souphttpsrc->link(next_element);
        /*
        souphttpsrc->signal_pad_added().connect(
                [&] (const Glib::RefPtr<Gst::Pad>& pad) {
                std::cout << "Pad name: " << pad->get_name() << std::endl;
                pad->link(next_element->get_static_pad("sink"));
                });
        */
    }
    return true;
}
void gst_task(string url, string multicast_addr, int port)
{
    RefPtr<Glib::MainLoop> loop;
    RefPtr<Gst::Pipeline>  pipeline;
    RefPtr<Gst::Element>   rndbuffersize;
    RefPtr<Gst::Element>   udpsink;
    sigc::connection m_timeout_connection;
    
    try{
        BOOST_LOG_TRIVIAL(info) 
            << "Start " 
            << url 
            << " --> udp://" 
            << multicast_addr 
            << ":" << port;
        loop = Glib::MainLoop::create();
        pipeline = Gst::Pipeline::create();
       
        rndbuffersize = Gst::ElementFactory::create_element("rndbuffersize");
        udpsink = Gst::ElementFactory::create_element("udpsink");
        
        if( !udpsink || !rndbuffersize){
            BOOST_LOG_TRIVIAL(error) << "Error in create for " << url;
            return;
        }
        pipeline->add(rndbuffersize)->add(udpsink);
        rndbuffersize->link(udpsink);
        if(!make_src_bin(url, pipeline, rndbuffersize)){
            BOOST_LOG_TRIVIAL(error) << "Error in create of Bin " << url;
            return;
        }

        rndbuffersize->set_property("min", 1316);
        rndbuffersize->set_property("max", 1316);

        udpsink->set_property("multicast-iface", string("lo"));
        udpsink->set_property("host", multicast_addr);
        udpsink->set_property("port", port);
        udpsink->set_property("sync", 1);

        pipeline->get_bus()->add_watch([loop](const RefPtr<Gst::Bus>&, 
                                             const RefPtr<Gst::Message>& msg){
                switch(msg->get_message_type()){
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
                BOOST_LOG_TRIVIAL(info) << cur/1000000;    
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
