#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#define BY_FFMPEG 1
using namespace std;
void gst_task(string in_multicast, int in_port, string hls_root);
void start_channel(json channel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = get_multicast(live_config, channel["input"]);
    string hls_root = string(HLS_ROOT) + channel["name"].get<string>();
    check_path(hls_root);
#if BY_FFMPEG
    hls_root += "/p.m3u8";
    auto cmd = boost::format("%s -i 'udp://%s:%d' "
            "-codec copy -map 0 -ac 2 "
            "-hls_time 4 -hls_list_size 10 -hls_flags delete_segments "
            " '%s'")
        % FFMPEG % in_multicast % INPUT_PORT % hls_root;
    exec_shell_loop(cmd.str());
#else
    gst_task(in_multicast, INPUT_PORT, hls_root); 
#endif
}
int main()
{
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    init();
    if(!get_live_config(live_config, "archive")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    check_path(HLS_ROOT);
    json silver_channels = json::parse(Mongo::find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["hls"] == true){
            if(chan["inputType"] != live_config.virtual_dvb_id &&
               chan["inputType"] != live_config.virtual_net_id  )
                pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
