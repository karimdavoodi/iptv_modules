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
#include <exception>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#include "db_structure.hpp"
using namespace std;

void gst_convert_udp_to_http(string in_multicast, int in_port, int http_stream_port);
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
    json channels = json::parse(db.find_mony("live_output_network", 
                R"({"active":true, "http":true})"));
    for(auto& chan : channels ){
        if(!Util::check_json_validity(db, "live_output_network", chan, 
                json::parse( live_output_network))) 
            continue;
        if(Util::chan_in_input(db, chan["input"], chan["inputType"])){
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
 *  @param channel : config of channel
 *  @param live_config : general live streamer config
 *
 * */
void start_channel(json channel, live_setting live_config)
{
    try{
        live_config.type_id = channel["inputType"];
        auto in_multicast = Util::get_multicast(live_config, channel["input"]);
        // TODO: find better way to calculate channel's port
        int tcpserver_port = 4000 + (channel["_id"].get<int64_t>() % 6000);
        LOG(debug) << "Start channle id:" << channel["_id"]
            << " on Port:" << tcpserver_port;
        while(true){
            //in_multicast = "229.3.229.201";
            gst_convert_udp_to_http(in_multicast, INPUT_PORT, tcpserver_port); 
            Util::wait(5000);
        }

    }catch(std::exception& e){
        LOG(error) << e.what();
    }
}
