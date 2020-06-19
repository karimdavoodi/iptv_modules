#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#define TEST_BY_FFMPEG 1
using namespace std;
void gst_task(string in_url, string out_multicast, int port);
void start_channel(string channel_str, live_setting live_config)
{
    json channel = json::parse(channel_str);
    BOOST_LOG_TRIVIAL(info) << "Start Channel: " << channel["name"];
    if(!channel["active"]){
        BOOST_LOG_TRIVIAL(info) << channel["name"] << " is not Active. Exit!";
        return;
    }
    if(!channel["static"]){
        BOOST_LOG_TRIVIAL(info) << channel["name"] << " is not Static. Exit!";
        return;
    }
    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
    auto url = channel["url"].get<string>();
#if TEST_BY_FFMPEG
    auto cmd = boost::format("%s -i '%s' -codec copy "
            " -f mpegts 'udp://%s:%d?pkt_size=1316' ")
        % FFMPEG % url % out_multicast % INPUT_PORT; 
    Util::exec_shell_loop(cmd.str());
#else
    //url = "rtsp://192.168.56.12:554/iptv/239.1.1.3/3200";  //for test
    //url = "http://192.168.56.12:8001/34/hdd11.ts";   // for test
    gst_task(url, out_multicast, INPUT_PORT);
#endif
}
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "network")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            string network = db.find_id("live_inputs_network", chan["input"]);
            pool.emplace_back(start_channel, network, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
