#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include "utils.hpp"
using namespace std;

void gst_task(string in_multicast, string out_multicast, int port);
void start_channel(json channel, live_setting live_config);

/*
 *   The main()
 *      - check license
 *      - read channels from mongoDB 
 *      - start thread for each active channel
 *      - wait to join
 * */
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "archive")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    Util::route_add(live_config.multicast_class, live_config.multicast_iface);
    json channels = json::parse(db.find_mony("live_output_network", 
                "{\"active\":true, \"udp\":true}"));
    for(auto& chan : channels ){
        IS_CHANNEL_VALID(chan);
        if(Util::chan_in_input(db, chan["input"], chan["inputType"])){
            pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
/*
 *  The channel thread function
 *  @param channel : config of channel
 *  @param live_config : general live streamer config
 *
 * */
void start_channel(json channel, live_setting live_config)
{
    LOG(info) << "Start Multicast for " << channel["name"];
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    auto out_multicast = Util::get_multicast(live_config, channel["_id"], true);
    while(true){
        gst_task(in_multicast, out_multicast, INPUT_PORT); 
        Util::wait(5000);
    }
}
