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
    auto in_multicast = get_multicast(live_config, channel["inputId"]);

    gst_task(in_multicast, INPUT_PORT, channel["_id"]); 
}
int main()
{
    vector<thread> pool;
    live_setting live_config;

    init();

    if(!get_live_config(live_config, "dvb")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }

    json silver_channels = json::parse(Mongo::find("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        // Active EPG only for DVB channels
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    BOOST_LOG_TRIVIAL(info) << "End!";
    return 0;
} 
