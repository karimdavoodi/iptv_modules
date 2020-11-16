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
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#include "db_structure.hpp"
#define BY_FFMPEG 0
using namespace std;

void gst_convert_udp_to_hls(string in_multicast, int in_port, string hls_root);
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
    if(!Util::get_live_config(db, live_config, "archive")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    if(getenv("HLS_CHAN") == nullptr){
        Util::system("rm -rf /opt/sms/tmp/HLS/*");
    }
    Util::check_path(HLS_ROOT);

    json channels = json::parse(db.find_mony("live_output_network", 
                R"({"active":true, "hls":true})"));
    for(auto& chan : channels ){
        if(!Util::check_json_validity(db, "live_output_network", chan, 
                json::parse( live_output_network))) 
            continue;
        if(Util::chan_in_input(db, chan["input"], chan["inputType"])){
            pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();

    Util::wait_forever();
} 
/*
 *  The channel thread function
 *  @param channel : config of channel
 *  @param live_config : general live streamer config
 *
 * */
void start_channel(json channel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    string chan_name = Util::get_channel_name(channel["input"], channel["inputType"]);
    string hls_root = string(HLS_ROOT) + chan_name;
    Util::check_path(hls_root);

    { // for test one channel
        char* env = getenv("HLS_CHAN");
        if(env != nullptr ){
            if(chan_name.find(env) == string::npos){
                LOG(warning) << "No test " << chan_name << " " << in_multicast;
                return;
            }else{
                LOG(warning) << "Test " << chan_name << " " << in_multicast;
            }
        }
    }
#if BY_FFMPEG
    auto cmd = boost::format("%s -i 'udp://%s:%d' "
            "-codec copy -map 0 -ac 2 "
            "-hls_time 4 -hls_list_size 10 -hls_flags delete_segments "
            " '%s/p.m3u8'")
        % FFMPEG % in_multicast % INPUT_PORT % hls_root;
    Util::exec_shell_loop(cmd.str());
#else
    while(true){
        Util::system("rm -f " + hls_root + "/*");
        gst_convert_udp_to_hls(in_multicast, INPUT_PORT, hls_root); 
        Util::wait(5000);
    }
#endif
}
