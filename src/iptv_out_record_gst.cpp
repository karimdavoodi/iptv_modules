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
#include <exception>
#include <thread>
#include <boost/filesystem.hpp>
#include <sys/statvfs.h>
#include "utils.hpp"
#include "gst.hpp"

#define DISK_FREE 0.1f
using namespace std;

struct Record_data {
    Gst::Data d;
    Mongo db;
    json channel;
    int maxPerChannel = 0;
};

void remove_old_timeshift(Mongo& db, int maxPerChannel, const json& channel);
gchararray splitmuxsink_location_cb(GstElement*  splitmux,
        guint fragment_id, gpointer data);
void tsdemux_pad_added_r(GstElement* object, GstPad* pad, gpointer data);

/*
 *   The Gstreamer main function
 *   Record udp:://in_multicast:port in storage as mkv
 *   
 *   @param channel: config of channel
 *   @param in_multicast : multicast of input stream
 *   @param port: output multicast port numper 
 *   @param maxPerChannel: expire time of recorded files 
 *
 * */
bool gst_convert_udp_to_mkv(json channel, string in_multicast, int port, int maxPerChannel)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start recode channel " << channel["name"] 
        << ": " << in_multicast 
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
                "async-finalize", true,
                "max-size-time", RECORD_DURATION * GST_SECOND,   
                "muxer-factory", "matroskamux", 
                nullptr);

        Gst::add_bus_watch(rdata.d);

        Gst::dot_file(rdata.d.pipeline, "iptv_record", 5);
        gst_element_set_state(GST_ELEMENT(rdata.d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(rdata.d.loop);
        return true;

    }catch(std::exception& e){
        LOG(error) << e.what();
        return false;
    }
}
void tsdemux_pad_added_r(GstElement* /*object*/, GstPad* pad, gpointer data)
{
    auto rdata = (Record_data *) data;
    Gst::demux_pad_link_to_muxer(rdata->d.pipeline, pad, 
            "mux", "audio_%u", "video");
}
string date_string()
{
    time_t now = time(nullptr);
    auto now_tm = localtime(&now);
    char buffer[100];
    strftime(buffer,sizeof(buffer),"%Y-%m-%d %H-%M-%S",now_tm);
    return string(buffer);
}
string get_media_path(bool is_tv, string name)
{
    string date = date_string();
    string file_path {MEDIA_ROOT};
    if(is_tv){
        file_path += "LiveVideo/" + name + " " + date + ".mkv"; 
    }else{
        file_path += "LiveAudio/" + name + " " + date + ".mp3"; 
    }
    return file_path;
}
void insert_content_info_db(Mongo &db,json& channel, uint64_t id)
{
    time_t now = time(nullptr);
    auto now_tm = localtime(&now);
    string name_date = date_string();

    string name = channel["name"];
    string file_name = name + name_date + ".mkv";
    json media = json::object();
    media["_id"] = id;
    media["format"] = CONTENT_FORMAT_MKV;
    media["type"] = channel["tv"].get<bool>() ?  
        CONTENT_TYPE_LIVE_VIDEO : CONTENT_TYPE_LIVE_AUDIO;
    media["price"] = 0;
    media["date"] = 1900 + now_tm->tm_year;
    media["time"] = now;
    media["languages"] = json::array();
    media["permission"] = channel["permission"];
    media["platform"] = json::array();
    media["category"] = channel["category"];
    media["description"] = {
        {"en",{
                  { "name" ,name_date },
                  { "description" ,"" }
              }},
        {"fa",{
                  { "name" ,name_date },
                  { "description" ,"" }
              }},
        {"ar",{
                  { "name" ,name_date },
                  { "description" ,"" }
              }}
    };
    media["name"] = get_media_path(channel["tv"], channel["name"]);
    db.insert("storage_contents_info", media.dump());
    LOG(info) << "Record " << name << ":" << name;
}
gchararray splitmuxsink_location_cb(GstElement*  /*splitmux*/,
        guint /*fragment_id*/, gpointer data)
{
    auto* rdata = (Record_data *) data;
    int64_t id = std::chrono::system_clock::now().time_since_epoch().count();
    try{
        insert_content_info_db(rdata->db, rdata->channel, id);
        remove_old_timeshift(rdata->db, rdata->maxPerChannel, rdata->channel);
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
    string file_path = get_media_path(rdata->channel["tv"], rdata->channel["name"]);
    LOG(info) << "For " << rdata->channel["name"] << " New file: " <<  file_path;
    return  g_strdup(file_path.c_str());
}
bool disk_is_full()
{
    struct statvfs s;
    if((statvfs(MEDIA_ROOT,&s)) >= 0 ) {
        float free = s.f_bfree * 1.0f / s.f_blocks;
        return free < DISK_FREE;
    }
    return false;
}
/*
 *   Clean Storage from recorded file that expired
 *
 *   @param maxPerChannel: time in houre to expire media files
 *   @param channel_name: name of channel
 *
 * */
void remove_old_timeshift(Mongo& db, int maxPerChannel, const json& channel)
{
    json filter;
    filter["name"] = channel["name"];
    filter["type"] = channel["tv"].get<bool>() 
        ? CONTENT_TYPE_LIVE_VIDEO 
        : CONTENT_TYPE_LIVE_AUDIO;
    long now = time(nullptr);

    LOG(debug) << "Remove old timeShift media before " << maxPerChannel;
    json channel_media = json::parse(db.find_mony("storage_contents_info", filter.dump()));

    LOG(trace) << "Filter:" << filter.dump(2) << " result count:" << channel_media.size();
    for(auto& media : channel_media){
        long media_date = (!media["time"].is_null() ? media["time"].get<long>() : 0);
        long late = now - media_date;
         
        if(late > maxPerChannel * RECORD_DURATION || disk_is_full() ){
            LOG(warning) << "Remove " << media["name"] << " of channel " << channel["name"] 
                << " for time " << media_date;
            uint64_t media_id = media["_id"];
            auto media_path = get_media_path(channel["tv"], channel["name"]);   
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
/*
 *   2020-09-14 17:02:19.365075 iptv_out_record 0x00007fbde17dd700 error: [element_link_request:226] Can'n link mpegvparse0:src to mux:video SRC PAD CAPS:video/mpeg, mpegversion=(int)[ 1, 2 ], parsed=(boolean)true, systemstream=(boolean)false

 * */
