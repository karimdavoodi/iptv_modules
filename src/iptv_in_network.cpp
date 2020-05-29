#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#define BY_FFMPEG 0
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
    auto out_multicast = get_multicast(live_config, channel["_id"]);

#if BY_FFMPEG
    auto cmd = boost::format("%s -i '%s' -codec copy "
            " -f mpegts 'udp://%s:%d?packesize=1316' ")
        % FFMPEG % channel["url"].get<string>() % out_multicast % INPUT_PORT; 

    BOOST_LOG_TRIVIAL(info) << cmd.str();
    std::system(cmd.str().c_str());
#else
    gst_task(channel["url"], out_multicast, INPUT_PORT);
#endif
}
int main()
{
    vector<thread> pool;
    live_setting live_config;
    
    CHECK_LICENSE;
    init();
    if(!get_live_config(live_config, "network")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            string network = Mongo::find_id("live_inputs_network", chan["inputId"]);
            pool.emplace_back(start_channel, network, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    while(true) this_thread::sleep_for(chrono::seconds(100));
    return 0;
} 
