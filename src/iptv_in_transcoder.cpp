#include <boost/format/format_fwd.hpp>
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
using nlohmann::json;
void gst_task(string media_path, string multicast_addr, int port);
void start_channel(json channel, live_setting live_config)
{
/*
        "_id": 1,
        "active": True,
        "name": "hdd1_ssd", 
        "input": 1, 
        "inputType": 2,
        "profile": 1,
 * */
    BOOST_LOG_TRIVIAL(info) << "Start Channel: " << channel["name"];
    //BOOST_LOG_TRIVIAL(trace) << channel.dump(4);
    if(!channel["active"]){
        BOOST_LOG_TRIVIAL(info) << channel["name"] << " is not Active. Exit!";
        return;
    }
    json profile = json::parse(Mongo::find_id("live_transcode_profile",channel["profile"])); 
    auto out_multicast = get_multicast(live_config, channel["_id"]);
    live_config.type_id = channel["inputType"];
    auto in_multicast  = get_multicast(live_config, channel["input"]);
    // TODO: do by Gst
   /* 
                "_id": 1,
                "name":"base_sd",
                "preset": "ultrafast",
                "videoCodec": "h264", 
                "videoSize": "720x576", 
                "videoRate": "2000k", 
                "videoFps": "24", 
                "videoProfile": "main", 
                "audioCodec": "aac", 
                "audioRate": "128k", 
                "extra": ""
    */
#if BY_FFMPEG 
    string vcodec = "copy" ,acodec = "copy";
    if(profile["videoCodec"].get<string>().find("264") != string::npos) 
        vcodec = "libx264";
    else if(profile["videoCodec"].get<string>().find("mpeg") != string::npos) 
        vcodec = "mpeg2video";
    else
        vcodec = profile["videoCodec"];

    acodec = profile["audioCodec"];
    auto opts = boost::format(" -preset %s -s %s -r %s -g %s -b:v %s -b:a %s   ")
            % profile["preset"] % profile["videoSize"] % profile["videoFps"] 
            % profile["videoFps"] % profile["videoRate"] % profile["audioRate"];
    auto codecs = boost::format("-vcodec %s -acodec %s") 
            % vcodec % acodec;

    auto cmd = boost::format("%s -i 'udp://%s:%d?reuse=1' "
            " %s %s -f mpegts 'udp://%s:%d?packesize=1316' ")
        % FFMPEG % in_multicast % INPUT_PORT % codecs.str() 
        % opts.str() % out_multicast % INPUT_PORT;

    BOOST_LOG_TRIVIAL(info) << cmd.str();
    std::system(cmd.str().c_str());
#else
    string in_uri = "udp://" + in_multicast + ":" + to_string(INPUT_PORT);
    gst_task(in_uri, multicast_out, INPUT_PORT);
#endif
    
}
int main()
{
    vector<thread> pool;
    live_setting live_config;

    init();
    if(!get_live_config(live_config, "transcode")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            json transcode = json::parse(Mongo::find_id("live_inputs_transcode", 
                        chan["inputId"]));
            IS_CHANNEL_VALID(transcode);
            pool.emplace_back(start_channel, transcode, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    return 0;
} 
