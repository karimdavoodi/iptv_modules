#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include "utils.hpp"
using namespace std;

void gst_task(Mongo& db, string in_multicast, int port, int chan_id);

void start_channel(json channel, live_setting live_config)
{
    Mongo db;
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    while(true){
        gst_task(db, in_multicast, INPUT_PORT, channel["_id"]); 
        Util::wait(3000);
    }
}
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "dvb")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        // Active EPG only for DVB channels
        if(chan["inputType"] == live_config.type_id){
            pool.emplace_back(start_channel, chan, live_config);
            //break;
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
