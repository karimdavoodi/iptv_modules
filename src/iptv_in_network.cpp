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
#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#include "db_structure.hpp"
#define TEST_BY_FFMPEG 0

using namespace std;

void gst_convert_stream_to_udp(string in_url, string out_multicast, int port);
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
    if(!Util::get_live_config(db, live_config, "network")){
        LOG(error) << "Error in live config! Exit.";
        return -1;
    }
    json channels = json::parse(db.find_mony("live_inputs_network", 
                "{\"active\":true}"));
    for(auto& chan : channels ){
        if(!Util::check_json_validity("live_inputs_network", chan, 
                json::parse( live_inputs_network))) 
            continue;
        if(chan["virtual"] || !chan["static"] || chan["webPage"] ) 
            continue;

        if(Util::chan_in_output(db, chan["_id"], live_config.type_id)){
            pool.emplace_back(start_channel, chan, live_config);
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
    LOG(info) << "Start Channel: " << channel["name"];
    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
    auto url = channel["url"].get<string>();
    while(true){
#if TEST_BY_FFMPEG
        auto cmd = boost::format("%s -i '%s' -codec copy "
                " -f mpegts 'udp://%s:%d?pkt_size=1316' ")
            % FFMPEG % url % out_multicast % INPUT_PORT; 
        Util::exec_shell_loop(cmd.str());
#else
        //url = "udp://229.1.1.1:3200";
        //url = "rtsp://192.168.56.12:554/iptv/239.1.1.2/3200"; 
        //url = "http://192.168.56.12:8001/34/hdd11.ts";   
        //url = "http://192.168.56.12/HLS/HLS/hdd11/p.m3u8";
        gst_convert_stream_to_udp(url, out_multicast, INPUT_PORT);
#endif
        Util::wait(5000);
    }
}
