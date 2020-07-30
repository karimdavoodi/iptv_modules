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

void gst_task(json channel, string in_multicast1, string in_multicast2, string out_multicast, int port);

void start_channel(json channel, live_setting live_config)
{
    Mongo db;

    LOG(info) << "Start mixed Channel: " << channel["name"];
    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
#if 0 // TEST
    channel["outputProfile"] = 100072;
    channel["input1"]["input"] = 1000671;
    channel["input1"]["inputType"] = 2;
    channel["input1"]["useVideo"] = true;
    channel["input1"]["useAudio"] = true;
    channel["input1"]["audioNumber"] = 1;

    channel["input2"]["input"] = 1000672;
    channel["input2"]["inputType"] = 2;
    channel["input2"]["useVideo"] = true;
    channel["input2"]["useAudio"] = true;
    channel["input2"]["audioNumber"] = 1;
    channel["input2"]["whiteTransparent"] = true;
    channel["input2"]["posX"] = 0;
    channel["input2"]["posY"] = 0;
    channel["input2"]["width"] = 400;
    channel["input2"]["height"] = 400;
#endif
    LOG(trace) << channel.dump(2);
    json profile = json::parse(db.find_id("live_profiles_mix", channel["profile"])); 
    if(profile.is_null() || profile["_id"].is_null()){
        LOG(error) << "Invalid mix profile!";
        LOG(debug) << profile.dump(2);
        return;
    } 
    channel["_profile"] = profile;

    live_config.type_id = channel["inputType1"];
    auto in_multicast1  = Util::get_multicast(live_config, channel["input1"]);
    live_config.type_id = channel["inputType2"];
    auto in_multicast2  = Util::get_multicast(live_config, channel["input2"]);

    while(true){
        gst_task(channel, in_multicast1, in_multicast2, out_multicast, INPUT_PORT);
        Util::wait(5000);
    }
}
int main(int argc, char** argv)
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "mix")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }

    json channels = json::parse(db.find_mony("live_inputs_mix", "{}"));
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
