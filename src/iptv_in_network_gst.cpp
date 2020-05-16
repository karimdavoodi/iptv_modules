#include <exception>
#include <gstreamermm.h>
#include <glibmm.h>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <boost/log/trivial.hpp>
#include "config.hpp"
#include "gstreamermm/caps.h"
#include "gstreamermm/pad.h"
#include "gstreamermm/query.h"

/*
            urisourcebin ---> queue1 -->  parsebin 
                        src_0 --> queue  --> mpegtsmux.video --> 
                        src_1 --> queue  --> mpegtsmux.audio -->  
                                       queue2 --> rndbuffersize ---> udpsink
 * */

using namespace std;
void gst_task(string in_url, string out_multicast, int port)
{
    using Glib::RefPtr;
    RefPtr<Glib::MainLoop> loop;
    RefPtr<Gst::Pipeline>  pipeline;
    RefPtr<Gst::Element>   urisourcebin;
    RefPtr<Gst::Element>   queue1;
    RefPtr<Gst::Element>   parsebin;
    RefPtr<Gst::Element>   mpegtsmux;
    RefPtr<Gst::Element>   queue2;
    RefPtr<Gst::Element>   queue_v;
    RefPtr<Gst::Element>   queue_a;
    RefPtr<Gst::Element>   rndbuffersize;
    RefPtr<Gst::Element>   udpsink;
    sigc::connection m_timeout_connection;
    try{
        BOOST_LOG_TRIVIAL(info) << in_url << " --> " 
            << out_multicast << ":" << port;

        loop = Glib::MainLoop::create();
        pipeline = Gst::Pipeline::create();

        urisourcebin = Gst::ElementFactory::create_element("urisourcebin");
        queue1  = Gst::ElementFactory::create_element("queue","urisourcebin_Q_parsebin");
        queue2  = Gst::ElementFactory::create_element("queue","tsmux_Q_rndbuffersize");
        queue_v  = Gst::ElementFactory::create_element("queue","Q_to_video_mux");
        queue_a  = Gst::ElementFactory::create_element("queue","Q_to_audio_mux");
        parsebin = Gst::ElementFactory::create_element("parsebin");
        mpegtsmux = Gst::ElementFactory::create_element("mpegtsmux");
        rndbuffersize = Gst::ElementFactory::create_element("rndbuffersize");
        udpsink = Gst::ElementFactory::create_element("udpsink");
        
        if( !urisourcebin || !udpsink ){
            BOOST_LOG_TRIVIAL(trace) << "Error in create";
            return;
        }
        try{
            pipeline->add(urisourcebin)->add(queue1)->add(parsebin)->
                add(mpegtsmux)->add(queue2)->add(rndbuffersize)->
                add(udpsink)->add(queue_a)->add(queue_v);

            // connect urisourcebin --> queue1 dynamicly
            queue1->link(parsebin);
            // connect parsebin --> mpegtsmux's queue dynamicly
            mpegtsmux->link(queue2)->link(rndbuffersize)->link(udpsink);
            queue_a->link(mpegtsmux);
            queue_v->link(mpegtsmux);

        } catch(std::runtime_error& e){
            BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
        }

        urisourcebin->set_property("uri", in_url);
        rndbuffersize->set_property("min", 1316);
        rndbuffersize->set_property("max", 1316);
        udpsink->set_property("multicast-iface", string("lo"));
        udpsink->set_property("host", out_multicast);
        udpsink->set_property("port", port);
        udpsink->set_property("sync", 1);

        urisourcebin->signal_pad_added().connect([&](const RefPtr<Gst::Pad>& pad){
                auto name = pad->get_name();
                BOOST_LOG_TRIVIAL(info) << "urisourcebin add pad: " << name;
                try{
                    if(!pad->can_link(queue1->get_static_pad("sink")))
                        BOOST_LOG_TRIVIAL(error) << "Can't link";
                    if(pad->link(queue1->get_static_pad("sink")) != Gst::PAD_LINK_OK){
                        BOOST_LOG_TRIVIAL(error) << "urisourcebin Not link";
                    }
                    pad->set_active();
                }catch(std::exception& e){
                    BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
                }
                });
        parsebin->signal_pad_added().connect([&](const RefPtr<Gst::Pad>& pad){
                try{
                auto caps = pad->query_caps(Gst::Caps::create_any());
                string pad_type = caps->get_structure(0).get_name();
                BOOST_LOG_TRIVIAL(info) 
                << "ParseBin add pad: " << pad->get_name()
                << " type:" << pad_type; 
                if(pad_type.find("video") != string::npos){
                    if(!pad->can_link(queue_v->get_static_pad("sink")))
                        BOOST_LOG_TRIVIAL(error) << "Can't link parsebin to queue";
                    else
                        pad->link(queue_v->get_static_pad("sink"));
                    pad->set_active();
                }
                if(pad_type.find("audio") != string::npos){
                    if(!pad->can_link(queue_a->get_static_pad("sink")))
                        BOOST_LOG_TRIVIAL(error) << "Can't link";
                    else
                        pad->link(queue_a->get_static_pad("sink"));
                    pad->set_active();
                }
                }catch(std::exception& e){
                    BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
                }
        });

        PIPLINE_WATCH;
        //PIPLINE_POSITION;

        pipeline->set_state(Gst::STATE_PLAYING);
        loop->run();
        pipeline->set_state(Gst::STATE_NULL);
        m_timeout_connection.disconnect();
        BOOST_LOG_TRIVIAL(info) << "Finish";
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(trace) << "Exception:" << e.what();
    }
}
