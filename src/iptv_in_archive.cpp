#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#define BY_FFMPEG 1
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
    BOOST_LOG_TRIVIAL(trace)  << "Check media time to play: "
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
void update_epg(int silver_chan_id, int content_id)
{
    json silver_chan = json::parse(Mongo::find_id("live_output_silver",silver_chan_id)); 
    json content_info = json::parse(Mongo::find_id("storage_contents_info",content_id)); 
    if(silver_chan["epg"].is_null())
        silver_chan["epg"] = json::array();
    else
        silver_chan["epg"].clear();
    json j;
    j["start"] = current_time();
    j["name"] = content_info["name"];
    j["duration"] = 1000; // FIXME: real duration!
    j["text"] = "";
    if(!content_info["description"].is_null() &&
            !content_info["description"]["en"].is_null() &&
            !content_info["description"]["en"]["description"].is_null() )
        j["text"] = content_info["description"]["en"]["description"];

    silver_chan["epg"].push_back(j);
    Mongo::replace_id("live_output_silver", silver_chan_id, silver_chan.dump());
}
void start_channel(json channel, int silver_chan_id, live_setting live_config)
{
    BOOST_LOG_TRIVIAL(info) << "Start Channel: " << channel["name"];
    if(!channel["active"]){
        BOOST_LOG_TRIVIAL(info) << channel["name"] << " is not Active. Exit!";
        return;
    }
    auto multicast = get_multicast(live_config, channel["_id"]);
    bool schedule = channel["manualSchedule"];

    while(true){
        for(auto& media : channel["contents"]){
            if(time_to_play(schedule, media)){
                BOOST_LOG_TRIVIAL(trace) << "Play media id: " << media["content"];

                update_epg(silver_chan_id, media["content"]);

                auto media_path = get_content_path(media["content"]);
#if BY_FFMPEG
                auto cmd = boost::format("%s -re -i '%s' -codec copy -f mpegts "
                        "'udp://%s:%d'")
                    % FFMPEG 
                    % media_path 
                    % multicast
                    % INPUT_PORT;
                BOOST_LOG_TRIVIAL(info) << cmd.str();
                std::system(cmd.str().c_str());
#else
                gst_task(media_path, multicast, INPUT_PORT);
#endif
            }else{
                BOOST_LOG_TRIVIAL(trace) << "Not play media id: " << media["content"]
                    << " due to time.";
            }
        }
        if(schedule) std::this_thread::sleep_for(std::chrono::seconds(5*60));
    }
}
int main()
{
    vector<thread> pool;
    live_setting live_config;
    
    CHECK_LICENSE;
    init();
    if(!get_live_config(live_config, "archive")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            json archive = json::parse(Mongo::find_id("live_inputs_archive", chan["inputId"]));
            IS_CHANNEL_VALID(archive);
            pool.emplace_back(start_channel, archive, chan["_id"], live_config);
            break;
        }
    }
    for(auto& t : pool)
        t.join();
    while(true) this_thread::sleep_for(chrono::seconds(100));
    return 0;
} 
