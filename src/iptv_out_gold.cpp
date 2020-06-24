#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include "utils.hpp"
using namespace std;
void gst_task(string in_multicast, string out_multicast);
void start_channel(json channel, live_setting live_config)
{
    LOG(error) << "TODO: implement GOLD channels ..." ;
    return;
}
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "archive")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    Util::route_add(live_config.multicast_class, live_config.multicast_iface);
    json gold_channels = json::parse(db.find_mony("live_output_gold", "{}"));
    for(auto& chan : gold_channels ){
        pool.emplace_back(start_channel, chan, live_config);
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
