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

using namespace std;
using nlohmann::json;

void gst_task(json channel, 
        string in_multicast1, 
        string in_multicast2, 
        string out_multicast, 
        int port);
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
        IS_CHANNEL_VALID(chan);
        
        if(Util::chan_in_output(db, chan["_id"], live_config.type_id)){
            pool.emplace_back(start_channel, chan, live_config);
            //break;
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
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

    LOG(info) << "Start mixed Channel: " << channel["name"];
    auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
#if 0 // TEST
    channel["outputProfile"] = 100072;
    channel["input1"]["input"] = 1000671;
    channel["input1"]["inputType"] = 2;
    channel["input1"]["useVideo"] = true;
    channel["input1"]["useAudio"] = true;
    channel["input1"]["audioNumber"] = 1;

    channel["input2"]["input"] = 1000672;
    channel["input2"]["inputType"] = 2;
    channel["input2"]["useVideo"] = true;
    channel["input2"]["useAudio"] = true;
    channel["input2"]["audioNumber"] = 1;
    channel["input2"]["whiteTransparent"] = true;
    channel["input2"]["posX"] = 0;
    channel["input2"]["posY"] = 0;
    channel["input2"]["width"] = 400;
    channel["input2"]["height"] = 400;
#endif
    LOG(trace) << channel.dump(2);
    json profile = json::parse(db.find_id("live_profiles_mix", channel["profile"])); 
    if(profile.is_null() || profile["_id"].is_null()){
        LOG(error) << "Invalid mix profile!";
        LOG(debug) << profile.dump(2);
        return;
    } 
    channel["_profile"] = profile;

    live_config.type_id = channel["inputType1"];
    auto in_multicast1  = Util::get_multicast(live_config, channel["input1"]);
    live_config.type_id = channel["inputType2"];
    auto in_multicast2  = Util::get_multicast(live_config, channel["input2"]);

    while(true){
        gst_task(channel, in_multicast1, in_multicast2, out_multicast, INPUT_PORT);
        Util::wait(5000);
    }
}
