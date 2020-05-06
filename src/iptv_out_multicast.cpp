#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <gstreamermm.h>
#include <boost/log/trivial.hpp>
#include "../third_party/json.hpp"
#include "mongo_driver.hpp"
#include "utils.hpp"
using namespace std;
using nlohmann::json;
void gst_task(string in_multicast, string out_multicast);

void start_channel(json channel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = get_multicast(live_config, channel["_id"]);
    auto out_multicast = get_multicast(live_config, channel["_id"], true);
    
}
int main()
{
    vector<thread> pool;
    live_setting live_config;
    Gst::init();
    if(!get_live_config(live_config, "archive")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        if(chan["active"] == true ){
            pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    return 0;
} 
