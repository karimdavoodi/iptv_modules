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

void gst_mix_two_udp_stream(string in_multicast1, string in_multicast2, 
                            string out_multicast, int port, json config);
void start_channel(json channel, live_setting live_config);

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
    if(!Util::get_live_config(db, live_config, "mix")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }

    json channels = json::parse(db.find_mony("live_inputs_mix", 
                "{\"active\":true}"));
    for(auto& chan : channels ){
        if(!Util::check_json_validity(db, "live_input_mix", chan, 
                json::parse( live_inputs_mix))) 
            continue;
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
    try{
        LOG(info) << "Start mixed Channel: " << channel["name"];
        auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
        LOG(trace) << channel.dump(2);

        json profile = json::parse(db.find_id("live_profiles_mix", channel["profile"])); 
        if(!Util::check_json_validity(db, "live_profiles_mix", profile, 
                    json::parse( live_profiles_mix))) 
            return;

        live_config.type_id = channel["inputType1"];
        auto in_multicast1  = Util::get_multicast(live_config, channel["input1"]);
        live_config.type_id = channel["inputType2"];
        auto in_multicast2  = Util::get_multicast(live_config, channel["input2"]);

        while(true){
            gst_mix_two_udp_stream(in_multicast1, in_multicast2, out_multicast, INPUT_PORT, profile);
            Util::wait(5000);
        }
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
}
