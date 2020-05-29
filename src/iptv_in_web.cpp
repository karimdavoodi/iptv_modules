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
void gst_task(string media_path, string multicast_addr, int port);
void start_channel(json channel, live_setting live_config)
{
    BOOST_LOG_TRIVIAL(info) << "Start web Channel: " << channel["name"];
    BOOST_LOG_TRIVIAL(error) << "TODO ..";
    // TODO: ...
}
int main()
{
    vector<thread> pool;
    live_setting live_config;

    CHECK_LICENSE;
    BOOST_LOG_TRIVIAL(info) << "TODO: iptv_in_web.";
    while(true) this_thread::sleep_for(chrono::seconds(100));

    init();
    if(!get_live_config(live_config, "web")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            json web_chan = json::parse(Mongo::find_id("live_inputs_web", 
                        chan["inputId"]));
            IS_CHANNEL_VALID(web_chan);
            pool.emplace_back(start_channel, web_chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    return 0;
} 
