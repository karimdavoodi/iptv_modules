#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/log/trivial.hpp>
#include "../third_party/json.hpp"
#include "iptv_archive_play.hpp"
#include "mongo_driver.hpp"
#include "utils.hpp"
using namespace std;
using nlohmann::json;

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
void start_channel(json& channel, live_setting live_config)
{
/*
        "_id": 1,
        "name": "hdd1",
        "active": True,
        "isTV": True, 
        "manualSchedule": False,
        "contents": [ 
                {
                "content": 1001,
                "weekday": 1,
                "time": 1,
                }
            ],
 * */
    BOOST_LOG_TRIVIAL(info) << "Start Channel: " << channel["name"];
    BOOST_LOG_TRIVIAL(trace) << channel.dump(4);
    if(!channel["active"]){
        BOOST_LOG_TRIVIAL(info) << channel["name"] << " is not Active. Exit!";
        return;
    }
    auto multicast = get_multicast(live_config, channel["_id"]);
    bool schedule = channel["manualSchedule"];

    while(true){
        for(auto& media : channel["contents"]){
            BOOST_LOG_TRIVIAL(trace) << "Play media id: " << media["content"];
            if(time_to_play(schedule, media)){
                auto media_path = get_content_path(media["content"]);
                IptvArchive archive(media_path, multicast, INPUT_PORT);
                archive.do_work();
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
    json channels = json::parse(Mongo::find("live_inputs_archive", "{}"));
    for(auto& chan : channels ){
        pool.emplace_back(start_channel, std::ref(chan), live_config);
    }
    for(auto& t : pool)
        t.join();
    return 0;
} 
