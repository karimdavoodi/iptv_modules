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
    LOG(info) << "Start unscramble Channel: " << channel["name"];
    LOG(error) << "TODO ..";
    // TODO: ...
}
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "unscramble")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            json unscrabmle_chan = json::parse(db.find_id("live_inputs_unscramble", 
                        chan["input"]));
            IS_CHANNEL_VALID(unscrabmle_chan);
            pool.emplace_back(start_channel, unscrabmle_chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
