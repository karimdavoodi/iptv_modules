// TODO: implement save stat of players for resume in restart
#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <gstreamermm.h>
#include <boost/log/trivial.hpp>
#include "../third_party/json.hpp"
#include "mongo_driver.hpp"
#include "utils.hpp"
using namespace std;
using nlohmann::json;
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
void start_channel(string channel_str, live_setting live_config)
{
    json channel = json::parse(channel_str);
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
                auto media_path = get_content_path(media["content"]);
                gst_task(media_path, multicast, INPUT_PORT);
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
    Gst::init();
    if(!get_live_config(live_config, "archive")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        if(chan["active"] == true && chan["inputType"] == live_config.type_id){
            string archive = Mongo::find_id("live_inputs_archive", chan["inputId"]);
            pool.emplace_back(start_channel, archive, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    return 0;
} 
