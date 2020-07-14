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

void init_display(int display_id);
void gst_task(string web_url, string out_multicast, int port);

void start_channel(json channel, live_setting live_config)
{
    LOG(info) << "Start web Channel: " << channel["name"];
    string url = channel["url"];
    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
    gst_task(url, out_multicast, INPUT_PORT);
}
int main(int argc, char** argv)
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "web")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    int n = 0, num = -1;
    if(argc == 2){
        // it is slave program. and start streaming for id 'num'
        num = atoi(argv[1]);
    }else{
        // it is master program. and start process for any channel
    }

    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            json web_chan = json::parse(db.find_id("live_inputs_web", chan["input"]));
            IS_CHANNEL_VALID(web_chan);
            n++;
            if(num < 0 ){
                LOG(info) << "Run app for Web channel " << web_chan["name"] << " by id " << n;
                string cmd = "/opt/sms/bin/iptv_in_web " + to_string(n) + " &";
                std::system(cmd.c_str());
            }else if(n == num){
                init_display(num);
                pool.emplace_back(start_channel, web_chan, live_config);
                break;
            }
            Util::wait(500);
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
