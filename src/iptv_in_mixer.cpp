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

void gst_task(json& channel, string out_multicast, int port);

void start_channel(json channel, live_setting live_config)
{
    LOG(info) << "Start mixed Channel: " << channel["name"];
    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
    gst_task(channel, out_multicast, INPUT_PORT);
}
int main(int argc, char** argv)
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "mixeded")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }

    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            json mixed_chan = json::parse(db.find_id("live_inputs_mixed", chan["input"]));
            IS_CHANNEL_VALID(mixed_chan);
            pool.emplace_back(start_channel, mixed_chan, live_config);
            break;
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
