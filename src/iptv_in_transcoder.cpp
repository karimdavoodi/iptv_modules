/*
 * Copyright (c) 2020 Karim, karimdavoodi@gmail.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <boost/format/format_fwd.hpp>
#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#define TEST_BY_FFMPEG 0

using namespace std;

void gst_transcode_of_stream(string in_multicast, int port, 
                    string out_multicast, json& profile);
const string profile_resolution(const string p_vsize);
void start_channel(json channel, live_setting live_config);

/*
 *   The main()
 *      - check license
 *      - read channels from mongoDB 
 *      - start thread for each active channel
 *      - wait to join
 * */
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "transcode")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    json channels = json::parse(db.find_mony("live_inputs_transcode",
                "{\"active\":true}"));
    for(const auto& chan : channels ){
        if(Util::chan_in_output(db, chan["_id"], live_config.type_id)){
            pool.emplace_back(start_channel, chan, live_config);
            //break;
        }
    }
    for(auto& t : pool)
        t.join();
    Util::wait_forever();
} 
/*
 *  The channel thread function
 *
 *  @param channel : config of channel
 *  @param live_config : general live streamer config
 *
 * */
void start_channel(json channel, live_setting live_config)
{
    Mongo db;
    LOG(info) << "Start transcoding of " << channel["name"] 
                  << " from input " << channel["input"];
    json profile = json::parse(db.find_id("live_profiles_transcode",channel["profile"])); 
    if(profile.is_null() || profile["_id"].is_null()){
        LOG(error) << "Invalid transcode profile!";
        LOG(debug) << profile.dump(2);
        return;
    } 

    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
    live_config.type_id = channel["inputType"];
    auto in_multicast  = Util::get_multicast(live_config, channel["input"]);
    LOG(trace) << profile.dump(2);
#if TEST_BY_FFMPEG 
    // TODO: apply  videoProfile and extra
    string vcodec = "copy" ,acodec = "copy", vsize = "720x576";
    string p_vcodec = profile["videoCodec"].get<string>();
    string p_acodec = profile["audioCodec"].get<string>();
    string p_vsize = profile["videoSize"].get<string>();
    if(p_vcodec.find("264") != string::npos)         vcodec = "libx264";
    else if(p_vcodec.find("mpeg2") != string::npos)  vcodec = "mpeg2video";
    else{
        LOG(error) << "Vicedo Codec not support " << p_vcodec 
            << " for channel " << channel["name"];
        return;
    }
    if(p_acodec.find("mp3") != string::npos)         acodec = "libmp3lame";
    else if(p_acodec.find("mp2") != string::npos)    acodec = "mp2";
    else if(p_acodec.find("aac") != string::npos)    acodec = "aac";
    else{
        LOG(error) << "Audio Codec not support " << p_acodec 
            << " for channel " << channel["name"];
        return;
    }
    vsize = profile_resolution(p_vsize);
    if(!vsize.size()){
        LOG(error) << "Video size not support " << p_vsize 
            << " for channel " << channel["name"];
        return;
    }
    auto opts = boost::format(" -preset %s -s %s -r %s -g %s -b:v %s -b:a %s   ")
            % profile["preset"] % vsize % profile["videoFps"] 
            % profile["videoFps"] % profile["videoRate"] % profile["audioRate"];
    string extra = profile["extra"];
    auto codecs = boost::format("-vcodec %s -acodec %s %s") 
            % vcodec % acodec % extra ;
    auto cmd = boost::format("%s -i 'udp://%s:%d?reuse=1' "
            " %s %s -f mpegts 'udp://%s:%d?pkt_size=1316' ")
        % FFMPEG % in_multicast % INPUT_PORT % codecs.str() 
        % opts.str() % out_multicast % INPUT_PORT;
    
    Util::exec_shell_loop(cmd.str());
#else
    while(true){
        LOG(info) << "Start transcoding of " << channel["name"] 
                  << " from input " << channel["input"];
        gst_transcode_of_stream(in_multicast, INPUT_PORT, out_multicast, profile);
        Util::wait(5000);
    }
#endif
    
}
