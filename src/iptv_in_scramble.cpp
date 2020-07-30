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
void gst_task(string in_multicast, int port, string out_multicast, 
        bool decrypt, string alg_name, string alg_key);

void start_channel(json channel, live_setting live_config)
{
    Mongo db;

    LOG(info) << "Start scramble Channel: " << channel["name"];

    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
    live_config.type_id = channel["inputType"];
    auto in_multicast  = Util::get_multicast(live_config, channel["input"]);

    json profile = json::parse(db.find_id("live_profiles_scramble", channel["profile"])); 
    if(profile.is_null() || profile["_id"].is_null()){
        LOG(error) << "Invalid scramble profile!";
        LOG(debug) << profile.dump(2);
        return;
    } 
    string algorithm_name = profile["offline"]["algorithm"];
    string algorithm_key  = profile["offline"]["key"];
    if(algorithm_name.size() > 0 && algorithm_key.size() > 0){
        gst_task(in_multicast, INPUT_PORT, out_multicast, channel["decrypt"], 
                algorithm_name, algorithm_key);
    }else{
        LOG(warning) << "Only support offline scrambling!";
    }
    
}
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "scramble")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    json channels = json::parse(db.find_mony("live_inputs_scramble", "{}"));
    for(auto& chan : channels ){
        IS_CHANNEL_VALID(chan);
        
        if(Util::chan_in_output(db, chan["_id"], live_config.type_id)){
            pool.emplace_back(start_channel, chan, live_config);
            //break;
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
