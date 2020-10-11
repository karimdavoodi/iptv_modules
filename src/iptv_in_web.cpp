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

using namespace std;
using nlohmann::json;

void gst_convert_web_to_stream(string web_url, string out_multicast, int port);
void start_channel(json channel, live_setting live_config);
void init_display(int display_id);

/*
 *   The main()
 *      - check license
 *      - read channels from mongoDB 
 *      - start thread for each active channel
 *      - wait to join
 * */
int main(int argc, char** argv)
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "network")){
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

    json channels = json::parse(db.find_mony("live_inputs_network",
                "{\"active\":true}"));
    for(auto& chan : channels ){
        if(!Util::check_json_validity(db, "live_inputs_network", chan, 
                json::parse( live_inputs_network))) 
            continue;
        if(chan["virtual"] || !chan["webPage"] ) 
            continue;

        if(Util::chan_in_output(db, chan["_id"], live_config.type_id)){
            n++;
            if(num < 0 ){
                LOG(info) << "Run app for Web channel " << chan["name"] << " by id " << n;
                string cmd = "/opt/sms/bin/iptv_in_web " + to_string(n) + " &";
                std::system(cmd.c_str());
            }else if(n == num){
                init_display(num);
                pool.emplace_back(start_channel, chan, live_config);
                break;
            }
            Util::wait(500);
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
    LOG(info) << "Start web Channel: " << channel["name"];
    string url = channel["url"];
    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
    while(true){
        gst_convert_web_to_stream(url, out_multicast, INPUT_PORT);
        Util::wait(5000);
    }
}
