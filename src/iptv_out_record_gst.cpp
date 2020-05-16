#include <exception>
#include <thread>
#include <gstreamermm.h>
#include <glibmm.h>
#include <boost/log/trivial.hpp>
#include "config.hpp"
using namespace std;
/*
gst-launch-1.0 -v udpsrc uri=udp://229.2.0.1:3200 ! queue \
    ! parsebin name=bin \
    bin. ! queue ! video/x-h264 ! \
           queue ! matroskamux name=mux ! filesink location=/tmp/11.ts \
    bin. ! queue ! audio/mpeg ! queue ! mux.  
 */
void gst_task(string  in_multicast, string file_path, int duration)
{
    using Glib::RefPtr;
    RefPtr<Glib::MainLoop> loop;
    RefPtr<Gst::Pipeline>  pipeline;
    RefPtr<Gst::Element>   udpsrc;
    RefPtr<Gst::Element>   queue;
    RefPtr<Gst::Element>   parsebin;
    RefPtr<Gst::Element>   mp4mux;
    RefPtr<Gst::Element>   filesink;
    sigc::connection m_timeout_connection;
    
    try{
        in_multicast += ":" + to_string(INPUT_PORT);
        BOOST_LOG_TRIVIAL(info) 
            << in_multicast << " --> " << file_path << " sec:" << duration;

        loop = Glib::MainLoop::create();
        pipeline = Gst::Pipeline::create();

        udpsrc = Gst::ElementFactory::create_element("udpsrc");
        queue  = Gst::ElementFactory::create_element("queue","udpsrc_Q_parsebin");
        parsebin = Gst::ElementFactory::create_element("parsebin");
        mp4mux = Gst::ElementFactory::create_element("matroskamux");
        filesink = Gst::ElementFactory::create_element("filesink");
        
        if( !udpsrc || !filesink ){
            BOOST_LOG_TRIVIAL(trace) << "Error in create";
            return;
        }
        
        pipeline->add(udpsrc)->add(queue)->add(parsebin)->add(mp4mux)->add(filesink);

        udpsrc->link(queue)->link(parsebin);// -> Q -> mp4mux
        mp4mux->link(filesink);
        

        udpsrc->set_property("uri", "udp://"+in_multicast);
        filesink->set_property("location", file_path);

        parsebin->signal_pad_added().connect([&](const RefPtr<Gst::Pad>& pad){
                auto name = pad->get_name();
                BOOST_LOG_TRIVIAL(info) << "ParseBin add pad: " << name;
                if(name == "src_0"){
                    auto queue = Gst::ElementFactory::create_element("queue","parsbin_Q_mp4V");
                    pipeline->add(queue);
                    pad->link(queue->get_static_pad("sink"));
                    queue->get_static_pad("src")->link(mp4mux->get_request_pad("video_%u"));
                    queue->sync_state_with_parent();
                }
                if(name == "src_1"){
                    auto queue = Gst::ElementFactory::create_element("queue","parsbin_Q_mp4A");
                    pipeline->add(queue);
                    pad->link(queue->get_static_pad("sink"));
                    queue->get_static_pad("src")->link(mp4mux->get_request_pad("audio_%u"));
                    queue->sync_state_with_parent();
                }
                });

        PIPLINE_WATCH;
        PIPLINE_POSITION;

        m_timeout_connection = Glib::signal_timeout().connect([&]()->bool {
                BOOST_LOG_TRIVIAL(info) <<  "End of duration";    
                //pipeline->send_event(Gst::EventEos::create());
                loop->quit();
                return false;
                }, duration*1000 );

        pipeline->set_state(Gst::STATE_PLAYING);
        loop->run();
        pipeline->set_state(Gst::STATE_NULL);
        m_timeout_connection.disconnect();
        BOOST_LOG_TRIVIAL(info) << "Finish";
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(trace) << "Exception:" << e.what();
    }
}
