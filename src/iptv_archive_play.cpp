#include <exception>
#include <boost/log/trivial.hpp>
#include "iptv_archive_play.hpp"

void IptvArchive::do_work()
{
    using Glib::RefPtr;
    RefPtr<Glib::MainLoop> loop;
    RefPtr<Gst::Pipeline>  pipeline;
    RefPtr<Gst::Element>   filesrc;
    RefPtr<Gst::Element>   tsparse;
    RefPtr<Gst::Element>   rndbuffersize;
    RefPtr<Gst::Element>   udpsink;
    
    // gst-launch-1.0 filesrc location=/tmp/2.ts ! 
    //      tsparse set-timestamps=1 ! 
    //      udpsink blocksize=1316 multicast-iface=lo host=239.1.1.2 port=3200 sync=true
    try{
        BOOST_LOG_TRIVIAL(trace) 
            << "Start " 
            << media_path 
            << " --> udp://" 
            << multicast_addr 
            << ":" << port;
        loop = Glib::MainLoop::create();
        pipeline = Gst::Pipeline::create();
        filesrc = Gst::ElementFactory::create_element("filesrc");
        tsparse = Gst::ElementFactory::create_element("tsparse");
        rndbuffersize = Gst::ElementFactory::create_element("rndbuffersize");
        udpsink = Gst::ElementFactory::create_element("udpsink");
        
        if( !filesrc || !tsparse || !udpsink || !rndbuffersize){
            BOOST_LOG_TRIVIAL(trace) << "Error in create";
            return;
        }
        pipeline->add(filesrc)->add(tsparse)->add(rndbuffersize)->add(udpsink);
        filesrc->link(tsparse);
        tsparse->link(rndbuffersize);
        rndbuffersize->link(udpsink);
        
        filesrc->set_property("location", media_path);
        tsparse->set_property("set-timestamps", 1);

        rndbuffersize->set_property("min", 1316);
        rndbuffersize->set_property("max", 1316);

        udpsink->set_property("multicast-iface", string("lo"));
        udpsink->set_property("host", multicast_addr);
        udpsink->set_property("port", port);
        udpsink->set_property("sync", 1);

        pipeline->get_bus()->add_watch([loop](const RefPtr<Gst::Bus>&, 
                                             const RefPtr<Gst::Message>& msg){
                //BOOST_LOG_TRIVIAL(trace) << msg->get_message_type();
                switch(msg->get_message_type()){
                    case Gst::MESSAGE_EOS:
                        loop->quit();
                        break;
                    default: break;
                }
                return true;
                });
        pipeline->set_state(Gst::STATE_PLAYING);
        loop->run();
        pipeline->set_state(Gst::STATE_NULL);
        BOOST_LOG_TRIVIAL(trace) << "Finish";

    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(trace) << "Exception:" << e.what();
    }
}
