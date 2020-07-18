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
void gst_task(string in_multicast, int port, string out_multicast, string key);

void start_channel(json channel, live_setting live_config)
{
    LOG(info) << "Start unscramble Channel: " << channel["name"];

    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
    live_config.type_id = channel["inputType"];
    auto in_multicast  = Util::get_multicast(live_config, channel["input"]);

    string biss_key = channel["bissKey"].is_null() ? "" : 
        channel["bissKey"].get<string>();
    biss_key = "123456123456";
    if(biss_key.size())
        gst_task(in_multicast, INPUT_PORT, out_multicast, biss_key);
    else
        LOG(warning) << "Not have BISS Key! not implement CCCAM";
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
            break;
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
