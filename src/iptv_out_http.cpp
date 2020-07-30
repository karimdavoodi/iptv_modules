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
            Util::wait(5000);
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
    json channels = json::parse(db.find_mony("live_output_network", "{\"active\":true}"));
    for(auto& chan : channels ){
        if(chan["http"] && Util::chan_in_input(db, chan["input"], chan["inputType"])){
            pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
