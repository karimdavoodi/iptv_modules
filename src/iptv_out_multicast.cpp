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
    BOOST_LOG_TRIVIAL(info) << "Start Multicast for " << channel["name"];
    live_config.type_id = channel["inputType"];
    auto in_multicast = get_multicast(live_config, channel["inputId"]);
    auto out_multicast = get_multicast(live_config, channel["_id"], true);
    gst_task(in_multicast, out_multicast); 
}
int main()
{
    vector<thread> pool;
    live_setting live_config;

    init();
    if(!get_live_config(live_config, "archive")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    route_add(live_config.multicast_class, live_config.multicast_iface);

    json silver_channels = json::parse(Mongo::find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["udp"] == true){
            if(chan["inputType"] != live_config.virtual_dvb_id &&
                    chan["inputType"] != live_config.virtual_net_id  )
            pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    while(true) this_thread::sleep_for(chrono::seconds(100));
    return 0;
} 
