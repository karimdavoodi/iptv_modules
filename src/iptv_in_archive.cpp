#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include <boost/filesystem/operations.hpp>
#include "utils.hpp"
#define TEST_BY_FFMPEG 0

using namespace std;
void gst_task(string media_path, string multicast_addr, int port);

bool time_to_play(bool schedule, json& media)
{
    if(!schedule) return true; // play anytime in non schedule mode
    int media_weekday = media["weekday"];
    int media_time = media["time"];
    
    auto now = time(NULL);
    auto now_tm = localtime(&now);
    auto now_wday = (now_tm->tm_wday + 1) % 7;
    auto now_hhmm = now_tm->tm_hour * 100 + now_tm->tm_min;
    LOG(debug)  << "Check media time to play: "
        << " now_wday " << now_wday
        << " now_hhmm " << now_hhmm
        << " media_weekday " << media_weekday
        << " media_time " << media_time;
    if(now_wday != media_weekday) return false;
    if(abs(now_hhmm - media_time) > 6) return false;
    return true;
}
int current_time()
{
    auto now = time(NULL);
    auto tm = localtime(&now);
    int  start = 0;
    start = 3600 * tm->tm_hour;
    start += 60 * tm->tm_min;
    start += tm->tm_sec;
    return start;
}
void update_epg(Mongo& db, int silver_chan_id, int content_id)
{
    json content_info = json::parse(db.find_id("storage_contents_info",content_id)); 
    json j;
    j["start"] = current_time();
    j["name"] = content_info["name"];
    j["duration"] = 1000; // FIXME: real duration!
    j["text"] = "";
    if(!content_info["description"].is_null() &&
            !content_info["description"]["en"].is_null() &&
            !content_info["description"]["en"]["description"].is_null() )
        j["text"] = content_info["description"]["en"]["description"];
    json epg = json::object();
    epg["_id"] = silver_chan_id;
    epg["total"] = 1;
    epg["content"] = json::array();
    epg["content"].push_back(j);
    db.insert_or_replace_id("live_output_silver_epg", silver_chan_id, epg.dump());
    LOG(info) << "Update EPG of channel_id:" << silver_chan_id;
}
void start_channel(json channel, int silver_chan_id, live_setting live_config)
{
    Mongo db;
    LOG(info) << "Start Channel: " << channel["name"];
    if(!channel["active"]){
        LOG(info) << channel["name"] << " is not Active. Exit!";
        return;
    }
    auto multicast = Util::get_multicast(live_config, channel["_id"]);
    bool schedule = channel["manualSchedule"];
    try{
        while(true){
           for(auto& media : channel["contents"]){
                if(time_to_play(schedule, media)){
                    LOG(debug) << "Play media id: " << media["content"];
                    update_epg(db, silver_chan_id, media["content"]);
                    auto media_path = Util::get_content_path(db, media["content"]);
                    if(media_path.size() == 0){
                        LOG(error) << "Invalid media path";
                        Util::wait(50000);
                        continue;
                    } 
                    if(!boost::filesystem::exists(media_path)){
                        LOG(error) << "Media not exists:" << media_path;
                        Util::wait(50000);
                        continue;
                    }
#if TEST_BY_FFMPEG
                    auto cmd = boost::format("%s -re -i '%s' -codec copy -f mpegts "
                            "'udp://%s:%d?pkt_size=1316'")
                        % FFMPEG 
                        % media_path 
                        % multicast
                        % INPUT_PORT;
                    Util::system(cmd.str());
#else
                    gst_task(media_path, multicast, INPUT_PORT);
#endif
                }else{
                    LOG(debug) << "Not play media id: " << media["content"]
                        << " due to time.";
                }
            }
            if(schedule) Util::wait(5*60*1000);
            else Util::wait(5000);
        }
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }

}
int main()
{
    vector<thread> pool;
    live_setting live_config;
    Mongo db;

    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "archive")){
        LOG(error) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            json archive = json::parse(db.find_id("live_inputs_archive", chan["input"]));
            IS_CHANNEL_VALID(archive);
            pool.emplace_back(start_channel, archive, chan["_id"], live_config);
            //break; //for test 
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
