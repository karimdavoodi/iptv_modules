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
    live_config.type_id = channel["inputType"];
    auto in_multicast = get_multicast(live_config, channel["input"]);
    BOOST_LOG_TRIVIAL(error) << "TODO: implement GOLD channels ..." ;
    return;
}
int main()
{
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    BOOST_LOG_TRIVIAL(info) << "TODO:.............";
    while(true) this_thread::sleep_for(chrono::seconds(100));
    init();
    if(!get_live_config(live_config, "archive")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    route_add(live_config.multicast_class, live_config.multicast_iface);
    json gold_channels = json::parse(Mongo::find_mony("live_output_gold", "{}"));
    for(auto& chan : gold_channels ){
        IS_CHANNEL_VALID(chan);
        pool.emplace_back(start_channel, chan, live_config);
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
