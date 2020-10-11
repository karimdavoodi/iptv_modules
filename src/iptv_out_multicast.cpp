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
#include "utils.hpp"
#include "db_structure.hpp"
using namespace std;

void gst_relay_udp_to_udp(string in_multicast, string out_multicast, int port);
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
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "archive")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    Util::add_route_by_mask8(live_config.multicast_class, 
                                live_config.multicast_iface);
    json channels = json::parse(db.find_mony("live_output_network", 
                "{\"active\":true, \"udp\":true}"));
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
    LOG(info) << "Start Multicast for " << channel["name"];
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    auto out_multicast = Util::get_multicast(live_config, channel["_id"], true);
    while(true){
        gst_relay_udp_to_udp(in_multicast, out_multicast, INPUT_PORT); 
        Util::wait(5000);
    }
}
