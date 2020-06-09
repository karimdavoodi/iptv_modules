#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include "utils.hpp"
using namespace std;
void gst_task(string in_multicast, int port, int chan_id);
void start_channel(json channel, live_setting live_config)
{
    auto in_multicast = get_multicast(live_config, channel["input"]);
    gst_task(in_multicast, INPUT_PORT, channel["_id"]); 
}
int main()
{
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    init();
    if(!get_live_config(live_config, "dvb")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        pool.emplace_back(start_channel, chan, live_config);
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 