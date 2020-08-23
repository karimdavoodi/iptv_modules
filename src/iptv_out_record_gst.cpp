/*
 * Copyright (c) 2020 Karim, karimdavoodi@gmail.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <thread>
#include <signal.h>
#include <boost/filesystem.hpp>
#include "utils.hpp"
#include "gst.hpp"

using namespace std;

struct Record_data {
    Gst::Data d;
    Mongo db;
    json channel;
    int maxPerChannel;
    Record_data(){}
};

void remove_old_timeshift(Mongo& db, int maxPerChannel, 
                                            const string channel_name);
gchararray splitmuxsink_location_cb(GstElement*  splitmux,
        guint fragment_id, gpointer data);
void tsdemux_pad_added_r(GstElement* object, GstPad* pad, gpointer data);

/*
 *   The Gstreamer main function
 *   Record udp:://in_multicast:port in storage as mp4
 *   
 *   @param channel: config of channel
 *   @param in_multicast : multicast of input stream
 *   @param port: output multicast port numper 
 *   @param maxPerChannel: expire time of recorded files 
 *
 * */
bool gst_convert_udp_to_mp4(json channel, string in_multicast, int port, int maxPerChannel)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start recode " << in_multicast 
        << " for " << maxPerChannel << " hour(s)"; 

    Record_data rdata;
    rdata.channel = channel;
    rdata.maxPerChannel = maxPerChannel;

    rdata.d.loop      = g_main_loop_new(nullptr, false);
    rdata.d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    try{
        auto udpsrc       = Gst::add_element(rdata.d.pipeline, "udpsrc"),
             queue_src    = Gst::add_element(rdata.d.pipeline, "queue", "queue_src"),
             tsdemux      = Gst::add_element(rdata.d.pipeline, "tsdemux"),
             splitmuxsink = Gst::add_element(rdata.d.pipeline, "splitmuxsink", "mux");

        gst_element_link_many(udpsrc, queue_src, tsdemux, nullptr);

        g_signal_connect(tsdemux, "pad-added", G_CALLBACK(tsdemux_pad_added_r), &rdata);
        g_signal_connect(splitmuxsink, "format-location", 
                G_CALLBACK(splitmuxsink_location_cb), &rdata);

        g_object_set(udpsrc, "uri", in_multicast.c_str(), nullptr);
        g_object_set(splitmuxsink, 
                "max-size-time", RECORD_DURATION * GST_SECOND,   
                "muxer-factory", "qtmux",
                nullptr);

        Gst::add_bus_watch(rdata.d);

        //Gst::dot_file(rdata.d.pipeline, "iptv_network", 5);
        gst_element_set_state(GST_ELEMENT(rdata.d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(rdata.d.loop);
        return true;

    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
        return false;
    }
}
void tsdemux_pad_added_r(GstElement* object, GstPad* pad, gpointer data)
{
    auto rdata = (Record_data *) data;
    Gst::demux_pad_link_to_muxer(rdata->d.pipeline, pad, 
            "mux", "video", "audio_%u");
}
gchararray splitmuxsink_location_cb(GstElement*  splitmux,
        guint fragment_id, gpointer data)
{
    Record_data* rdata = (Record_data *) data;
    int64_t id = std::chrono::system_clock::now().time_since_epoch().count();
    Util::insert_content_info_db(rdata->db, rdata->channel, id);
    remove_old_timeshift(rdata->db, rdata->maxPerChannel, rdata->channel["name"]);

    string file_path = MEDIA_ROOT "TimeShift/" + to_string(id) + ".mp4"; 
    LOG(trace) << "For " << rdata->channel["name"]
               << " New file: " <<  file_path;
    return  g_strdup(file_path.c_str());
}
/*
 *   Clean Storage from recorded file that expired
 *
 *   @param maxPerChannel: time in houre to expire media files
 *   @param channel_name: name of channel
 *
 * */
void remove_old_timeshift(Mongo& db, int maxPerChannel, const string channel_name)
{
    json filter;
    filter["name"] = channel_name;
    filter["type"] = TIME_SHIFT_TYPE;
    long now = time(nullptr);

    LOG(debug) << "Remove old timeShift media before " << maxPerChannel;
    json channel_media = json::parse(db.find_mony("storage_contents_info", filter.dump()));

    LOG(trace) << "Filter:" << filter.dump(2) << " result count:" << channel_media.size();
    for(auto& media : channel_media){
        long media_date = media["date"];
        long late = now - media_date;
        if(late > maxPerChannel * RECORD_DURATION ){
            LOG(warning) << "Remove " << media["name"] << " of channel " << channel_name 
                << " for time " << media_date;
            uint64_t media_id = media["_id"];
            auto media_path = MEDIA_ROOT "TimeShift/" + 
                                                to_string(media_id) + ".mp4";
            db.remove_id("storage_contents_info", media_id);
            if(boost::filesystem::exists(media_path)){
                boost::filesystem::remove(media_path);
                LOG(info) << "TimeShift file removed:" << media_path;
            }else{
                LOG(error) << "TimeShift file not exists:" << media_path;
            }
        }
    }
}
