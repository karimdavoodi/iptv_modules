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
    json profile = json::parse(db.find_id("live_transcode_profile",
                channel["outputProfile"])); 
    channel["out_width"] = DEFAULT_WIDTH;
    channel["out_height"] = DEFAULT_HEIGHT;
    if(profile["_id"].is_null()){
        LOG(error) << "transcode profile id is not valid, use default";
    }else{
        auto resolution = Util::profile_resolution_pair(profile["videoSize"]);
        if(resolution.first > 0){
            channel["out_width"] = resolution.first; 
            channel["out_height"] = resolution.second;
        }
    }
    LOG(debug) << "Output resolution:" << channel["out_width"] << 
                "x" << channel["out_height"] ;
    live_config.type_id = channel["input1"]["inputType"];
    auto in_multicast1  = Util::get_multicast(live_config, channel["input1"]["input"]);
    live_config.type_id = channel["input2"]["inputType"];
    auto in_multicast2  = Util::get_multicast(live_config, channel["input2"]["input"]);

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
    if(!Util::get_live_config(db, live_config, "mixed")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }

    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            json mixed_chan = json::parse(db.find_id("live_inputs_mixed", chan["input"]));
            IS_CHANNEL_VALID(mixed_chan);
            pool.emplace_back(start_channel, mixed_chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
