#include <exception>
#include <thread>
#include <map>
#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <gst/mpegts/mpegts.h>
#include <gstreamermm.h>
#include <glibmm.h>
#include <boost/log/trivial.hpp>
#include "config.hpp"
#include "gst/gstmessage.h"
#include "utils.hpp"
using namespace std;
/*
   gst-launch-1.0 -v udpsrc uri=udp://229.2.0.1:3200 ! tsparse ! fakesink
   */

struct Event {
    string name;
    string text;
    int start;
    int duration;
};
map<int, Event> day_eit;

struct DayTime {
    int hour, minute, second;
};
int gst_date_to_int(GstDateTime* date)
{
    int n = 0;
    n  = 3600 * gst_date_time_get_hour(date);
    n += 60 * gst_date_time_get_minute(date);
    n += gst_date_time_get_second(date);
    return n;
}
void gst_date_to_day_time(GstDateTime* date, DayTime& d)
{
    d.hour = gst_date_time_get_hour(date);
    d.minute = gst_date_time_get_minute(date);
    d.second = gst_date_time_get_second(date);
}
string gst_date_to_str(GstDateTime* date)
{
    string ret = "";
    if(date){
        DayTime d;
        gst_date_to_day_time(date, d);
        ret = to_string(d.hour) + ":" + to_string(d.minute) + ":" + to_string(d.second);
    }
    return ret;
}
#if 0
DayTime current_time;
void dump_tdt (GstMpegtsSection * section)
{
    GstDateTime *date = gst_mpegts_section_get_tdt (section);
    if (date) {
        gst_date_to_day_time(date, current_time);
        gst_date_time_unref (date);
    }
}
void dump_tot (GstMpegtsSection * section)
{
    const GstMpegtsTOT *tot = gst_mpegts_section_get_tot (section);

    if (tot) {
        gst_date_to_day_time(tot->utc_time, current_time);
        //      gst_mpegts_section_unref(tot);
    }
}
*/
#endif
void dump_descriptors (GstMpegtsEITEvent *event)
{
    GPtrArray *descriptors = event->descriptors;
    for (int i = 0; i < descriptors->len; i++) {
        GstMpegtsDescriptor *desc = (GstMpegtsDescriptor *)
                                    g_ptr_array_index (descriptors, i);
        if(desc->tag == GST_MTS_DESC_DVB_SHORT_EVENT) {
            gchar *language_code, *event_name, *text;
            if (gst_mpegts_descriptor_parse_dvb_short_event (desc, &language_code,
                        &event_name, &text)) {
                Event e;
                e.start = gst_date_to_int(event->start_time);            
                e.duration = event->duration;
                e.name = event_name; 
                e.text = text;
                day_eit[e.start] = e;
                g_free (language_code);
                g_free (event_name);
                g_free (text);
            }
        }
    }
}

void dump_eit(GstMpegtsSection *sec)
{
    const GstMpegtsEIT *eit = gst_mpegts_section_get_eit(sec);
    if(eit == NULL){
        BOOST_LOG_TRIVIAL(warning) << "Can't parse mpegts EIT";
        return;
    } 
    BOOST_LOG_TRIVIAL(trace) << "EIT:"
        << " section_id " << sec->subtable_extension
        << " transport_stream_id " << eit->transport_stream_id
        << " original_network_id " << eit->original_network_id
        << " segment_last_section_number " << eit->segment_last_section_number
        << " last_table_id " << eit->last_table_id
        << " actual_stream " << (eit->actual_stream ? "true" : "false") 
        << " present_following " << (eit->present_following ? "TRUE" : "FALSE");

    int len = eit->events->len;
    BOOST_LOG_TRIVIAL(trace) << "Event number:" << len;
    for (int i = 0; i < len; i++) {
        GstMpegtsEITEvent *event = (GstMpegtsEITEvent *)
            g_ptr_array_index(eit->events, i);
        if(event->running_status != 0) continue;
        BOOST_LOG_TRIVIAL(trace) 
            <<  event->running_status
            << " event_id:" << event->event_id
            << " start_time:" << gst_date_to_str(event->start_time)
            << " duration:" << event->duration;
        dump_descriptors (event);
    }
    //g_object_unref(GST_OBJECT(eit));
    
}
void channel_epg_update(map<int, Event>& day_eit, int channel_id)
{
    json eit = json::array();
    for(auto &event : day_eit){
        json j;
        j["start"] = event.second.start;
        j["name"] = event.second.name;
        j["duration"] = event.second.duration;
        j["text"] = event.second.text;
        eit.push_back(j);

        BOOST_LOG_TRIVIAL(trace) 
            <<  "Start:" << event.second.start
            <<  " Name:" << event.second.name
            <<  " Text:" << event.second.text
            <<  " Duration:" << event.second.duration;
    }
    
    day_eit.clear();
    json silver_channel = json::parse(Mongo::find_id("live_output_silver", channel_id));
    silver_channel["epg"] = eit;
    Mongo::replace_by_id("live_output_silver", channel_id, silver_channel.dump());
    BOOST_LOG_TRIVIAL(info) << "Update EPG of channel_id:" << channel_id;
}
void gst_task(string in_multicast, int port, int channel_id)
{
    using Glib::RefPtr;
    RefPtr<Glib::MainLoop> loop;
    RefPtr<Gst::Pipeline>  pipeline;
    RefPtr<Gst::Element>   udpsrc;
    RefPtr<Gst::Element>   tsparse;
    RefPtr<Gst::Element>   fakesink;

    try{
        in_multicast = "udp://" + in_multicast + ":" + to_string(INPUT_PORT);
        BOOST_LOG_TRIVIAL(info) 
            << in_multicast << " --> EPG";

        loop = Glib::MainLoop::create();
        pipeline = Gst::Pipeline::create();

        udpsrc = Gst::ElementFactory::create_element("udpsrc");
        tsparse = Gst::ElementFactory::create_element("tsparse");
        fakesink = Gst::ElementFactory::create_element("fakesink");

        if( !udpsrc || !fakesink || !tsparse ){
            BOOST_LOG_TRIVIAL(trace) << "Error in create";
            return;
        }

        pipeline->add(udpsrc)->add(tsparse)->add(fakesink);
        udpsrc->link(tsparse)->link(fakesink);

        udpsrc->set_property("uri", in_multicast);

        pipeline->get_bus()->add_watch([loop](const RefPtr<Gst::Bus>&, 
                    const RefPtr<Gst::Message>& msg){
                BOOST_LOG_TRIVIAL(trace) 
                    << "Got Msg: " << msg->get_message_type()
                    << " From:  " << msg->get_source()->get_name()
                    << " Struct: " <<  msg->get_structure().to_string();
                GstMessage *m = msg->gobj();
                GstMpegtsSection *sec;
                switch(msg->get_message_type()){
                    case Gst::MESSAGE_ELEMENT:
                        sec = gst_message_parse_mpegts_section(m);
                        if(sec == NULL){
                            BOOST_LOG_TRIVIAL(warning) << "Can't parse mpegts section";
                            return true;
                        }
                        if(sec->section_type == GST_MPEGTS_SECTION_EIT)
                            dump_eit(sec); 
                        gst_mpegts_section_unref (sec);
#if 0
                        if(sec->section_type == GST_MPEGTS_SECTION_TOT)
                            dump_tot(sec); 
                        if(sec->section_type == GST_MPEGTS_SECTION_TDT)
                            dump_tdt(sec); 
#endif
                        break;
                    case Gst::MESSAGE_ERROR:
                        BOOST_LOG_TRIVIAL(error) << 
                            RefPtr<Gst::MessageError>::cast_static(msg)->parse_debug();
                        break;
                    case Gst::MESSAGE_EOS:
                        BOOST_LOG_TRIVIAL(info) <<  "Got EOS";    
                        loop->quit();
                        break;
#if 0
                    case Gst::MESSAGE_TAG:
                        RefPtr<Gst::MessageTag>::cast_static(msg)
                            ->parse_tag_list().foreach(
                                [](const Glib::ustring name){
                                BOOST_LOG_TRIVIAL(info) << "Tag:" <<  name;
                                });
                        break;
#endif
                    default: break;
                }
                return true;
        });
        sigc::connection m_timeout_pos = Glib::signal_timeout().connect([&]()->bool {
                try{
                    channel_epg_update(day_eit, channel_id);
                }catch(std::exception& e){
                    BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what();
                }
                return true;
                }, EPG_UPDATE_TIME );

        pipeline->set_state(Gst::STATE_PLAYING);
        loop->run();
        pipeline->set_state(Gst::STATE_NULL);
        BOOST_LOG_TRIVIAL(info) << "Finish";
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(trace) << "Exception:" << e.what();
    }
}
