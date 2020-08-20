#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#define BY_FFMPEG 0
using namespace std;
void gst_task(string in_multicast, int in_port, string hls_root);
void start_channel(json channel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    string hls_root = string(HLS_ROOT) + channel["name"].get<string>();
    Util::check_path(hls_root);
#if BY_FFMPEG
    auto cmd = boost::format("%s -i 'udp://%s:%d' "
            "-codec copy -map 0 -ac 2 "
            "-hls_time 4 -hls_list_size 10 -hls_flags delete_segments "
            " '%s/p.m3u8'")
        % FFMPEG % in_multicast % INPUT_PORT % hls_root;
    Util::exec_shell_loop(cmd.str());
#else
    while(true){
        gst_task(in_multicast, INPUT_PORT, hls_root); 
        Util::wait(5000);
    }
#endif
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
    json channels = json::parse(db.find_mony("live_output_network", 
                "{\"active\":true, \"hls\":true}"));
    for(auto& chan : channels ){
        if(Util::chan_in_input(db, chan["input"], chan["inputType"])){
            pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();

    THE_END;
} 
