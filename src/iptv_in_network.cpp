#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#define TEST_BY_FFMPEG 0

using namespace std;
void gst_task(string in_url, string out_multicast, int port);

void start_channel(json channel, live_setting live_config)
{
    LOG(info) << "Start Channel: " << channel["name"];
    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
    auto url = channel["url"].get<string>();
    while(true){
#if TEST_BY_FFMPEG
        auto cmd = boost::format("%s -i '%s' -codec copy "
                " -f mpegts 'udp://%s:%d?pkt_size=1316' ")
            % FFMPEG % url % out_multicast % INPUT_PORT; 
        Util::exec_shell_loop(cmd.str());
#else
        //url = "udp://229.1.1.1:3200";
        //url = "rtsp://192.168.56.12:554/iptv/239.1.1.2/3200"; 
        //url = "http://192.168.56.12:8001/34/hdd11.ts";   
        //url = "http://192.168.56.12/HLS/HLS/hdd11/p.m3u8";
        gst_task(url, out_multicast, INPUT_PORT);
#endif
        Util::wait(5000);
    }
}
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "network")){
        LOG(error) << "Error in live config! Exit.";
        return -1;
    }
    json channels = json::parse(db.find_mony("live_inputs_network", "{}"));
    for(auto& chan : channels ){
        IS_CHANNEL_VALID(chan);
        
        if(chan["virtual"] || !chan["static"] || chan["webPage"] ) 
            continue;

        if(Util::chan_in_output(db, chan["_id"], live_config.type_id)){
            pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
