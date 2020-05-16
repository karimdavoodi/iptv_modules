#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
using namespace std;
using nlohmann::json;
void gst_task(string media_path, string multicast_addr, int port);
void start_channel(json channel, live_setting live_config)
{
/*
 * */
    //BOOST_LOG_TRIVIAL(trace) << channel.dump(4);
    if(!channel["active"]){
        BOOST_LOG_TRIVIAL(info) << channel["name"] << " is not Active. Exit!";
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "Start UnScramble Channel: " << channel["name"];
    BOOST_LOG_TRIVIAL(error) << "TODO ..";
    // TODO: ...
}
int main()
{
    vector<thread> pool;
    live_setting live_config;

    init();
    if(!get_live_config(live_config, "scramble")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        if(chan["active"] == true && chan["inputType"] == live_config.type_id){
            json transcode = json::parse(Mongo::find_id("live_unscramble", 
                        chan["inputId"]));
            pool.emplace_back(start_channel, transcode, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    return 0;
} 
