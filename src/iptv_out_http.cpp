#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
using namespace std;
void gst_task(string in_multicast, int in_port, int http_stream_port, const string ch_name);
void start_channel(json channel, live_setting live_config)
{
    try{
        live_config.type_id = channel["inputType"];
        auto in_multicast = Util::get_multicast(live_config, channel["input"]);
        // TODO: find better way to calculate channel's port
        int tcpserver_port = 4000 + (channel["_id"].get<int>() % 6000);
        LOG(info) << "Start channle " << channel["name"] << " id:" << channel["_id"]
            << " on Port:" << tcpserver_port;
        while(true){
            gst_task(in_multicast, INPUT_PORT, tcpserver_port, 
                    channel["name"].get<string>()); 
            Util::wait(1000);
        }

    }catch(std::exception& e){
        LOG(error) << e.what();
    }
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
    Util::check_path(HLS_ROOT);
    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["http"] == true){
            if(chan["inputType"] != live_config.virtual_dvb_id &&
               chan["inputType"] != live_config.virtual_net_id  ){
                pool.emplace_back(start_channel, chan, live_config);
                //break;
            }
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
