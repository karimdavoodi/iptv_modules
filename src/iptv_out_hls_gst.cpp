#include <exception>
#include <thread>
#include <gstreamermm.h>
#include <glibmm.h>
#include <boost/log/trivial.hpp>
#include "gst.hpp"
using namespace std;
/*
gst-launch-1.0 -v --gst-debug-level=3  udpsrc uri=udp://229.2.0.1:3200 \
    ! queue ! parsebin name=demux \
    ! video/x-h264 ! h264parse \
    ! queue ! hlssink2  name=sink \
    #demux. ! queue ! audio/mpeg ! aacparse ! sink.
 */
void gst_task(string in_multicast, int in_port, string hls_root)
{
    using Glib::RefPtr;
    RefPtr<Glib::MainLoop> loop;
    RefPtr<Gst::Pipeline>  pipeline;
    RefPtr<Gst::Element>   udpsrc;
    RefPtr<Gst::Element>   parsebin;
    RefPtr<Gst::Element>   queue1;
    RefPtr<Gst::Element>   queue_v;
    RefPtr<Gst::Element>   queue_a;
    RefPtr<Gst::Element>   hlssink;
    sigc::connection m_timeout_connection;
   
    try{
        in_multicast = "229.2.0.1";
        in_multicast += ":" + to_string(in_port);
        BOOST_LOG_TRIVIAL(info) 
            << "Start " 
            << in_multicast 
            << " --> HLS in " << hls_root; 
        loop = Glib::MainLoop::create();
        pipeline = Gst::Pipeline::create();
        
        udpsrc = Gst::ElementFactory::create_element("udpsrc");
        queue1 = Gst::ElementFactory::create_element("queue");
        parsebin = Gst::ElementFactory::create_element("parsebin");
        queue_v = Gst::ElementFactory::create_element("queue","queue_v");
        queue_a = Gst::ElementFactory::create_element("queue", "queue_a");
        hlssink = Gst::ElementFactory::create_element("hlssink2");
        
        if( !udpsrc || !hlssink || !parsebin ){
            BOOST_LOG_TRIVIAL(debug) << "Error in create";
            return;
        }
        try{
            pipeline->add(udpsrc)->add(queue1)->add(parsebin)
                ->add(queue_v)->add(queue_a)->add(hlssink);
            udpsrc->link(queue1)->link(parsebin);
            queue_v->get_static_pad("src")->link(hlssink->get_request_pad("video"));
            queue_a->get_static_pad("src")->link(hlssink->get_request_pad("audio"));
            // link parsebin->queue_v  in video
            // link parsebin->queue_a  in audio
        }catch(std::exception& e){
            BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
        }
        
        
        udpsrc->set_property("uri", "udp://"+in_multicast);
        parsebin->signal_pad_added().connect([&](const RefPtr<Gst::Pad>& pad){
                try{
                
                    auto caps = pad->query_caps(Gst::Caps::create_any());
                    string caps_name =  caps->get_structure(0).get_name();
                    BOOST_LOG_TRIVIAL(info) << "add Pad:" << pad->get_name()
                            << " Caps:" << caps_name;
                    if(caps_name.find("video") != string::npos){
                      if(pad->link(queue_v->get_static_pad("sink")) != Gst::PAD_LINK_OK)
                        BOOST_LOG_TRIVIAL(info) << "error: parsebin_v -> queue_v"; 
                      else
                        BOOST_LOG_TRIVIAL(info) << "Link: parsebin_v -> queue_v"; 
                    }else if(caps_name.find("audio") != string::npos){
                      if(pad->link(queue_a->get_static_pad("sink")) != Gst::PAD_LINK_OK)
                        BOOST_LOG_TRIVIAL(info) << "error: parsebin_a -> queue_a"; 
                      else
                        BOOST_LOG_TRIVIAL(info) << "Link: parsebin_a -> queue_a"; 
                    }else{
                        BOOST_LOG_TRIVIAL(info) << "Not link PAD"; 
                    }
                    pad->set_active();
                }catch(std::exception& e){
                    BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
                }
                });
        //hlssink->set_property("multicast-iface", string("lo"));
        //PIPLINE_WATCH;
        //PIPLINE_POSITION;
        pipeline->set_state(Gst::STATE_PLAYING);
        loop->run();
        pipeline->set_state(Gst::STATE_NULL);
        m_timeout_connection.disconnect();
        BOOST_LOG_TRIVIAL(debug) << "Finish";
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
    }
}
