#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <gstreamermm.h>
#include <boost/log/trivial.hpp>
#include "../third_party/json.hpp"
#include "iptv_transcoder_gst.hpp"
#include "mongo_driver.hpp"
#include "utils.hpp"
using namespace std;
using nlohmann::json;
void gst_task(string media_path, string multicast_addr, int port);
void start_channel(json channel, live_setting live_config)
{
/*
        "_id": 1,
        "active": True,
        "name": "hdd1_ssd", 
        "input": 1, 
        "inputType": 2,
        "profile": 1,
 * */
    BOOST_LOG_TRIVIAL(info) << "Start Channel: " << channel["name"];
    //BOOST_LOG_TRIVIAL(trace) << channel.dump(4);
    if(!channel["active"]){
        BOOST_LOG_TRIVIAL(info) << channel["name"] << " is not Active. Exit!";
        return;
    }
    json profile = json::parse(Mongo::find_id("live_transcode_profile",channel["profile"])); 
    auto multicast_out = get_multicast(live_config, channel["_id"]);
    live_config.type_id = channel["inputType"];
    auto multicast_in  = get_multicast(live_config, channel["input"]);
    string in_uri = "udp://" + multicast_in + ":" + to_string(INPUT_PORT);
    gst_task(in_uri, multicast_out, INPUT_PORT);
}
int main()
{
    vector<thread> pool;
    live_setting live_config;
    Gst::init();
    if(!get_live_config(live_config, "transcode")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        if(chan["active"] == true && chan["inputType"] == live_config.type_id){
            json transcode = json::parse(Mongo::find_id("live_inputs_transcode", 
                        chan["inputId"]));
            pool.emplace_back(start_channel, transcode, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    return 0;
} 
