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
using nlohmann::json;

void gst_mpegts_crypto(
        string in_multicast, 
        int port, 
        string out_multicast, 
        bool decrypt, 
        string alg_name, 
        string alg_key);
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
    if(!Util::get_live_config(db, live_config, "scramble")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    json channels = json::parse(db.find_mony("live_inputs_scramble",
                "{\"active\":true}"));
    for(auto& chan : channels ){
        if(!Util::check_json_validity(db, "live_inputs_scramble", chan, 
                json::parse( live_inputs_scramble))) 
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
    Mongo db;

    try{
        LOG(info) << "Start scramble Channel: " << channel["name"];
        auto out_multicast = Util::get_multicast(live_config, channel["_id"]);
        live_config.type_id = channel["inputType"];
        auto in_multicast  = Util::get_multicast(live_config, channel["input"]);

        json profile = json::parse(db.find_id("live_profiles_scramble", channel["profile"])); 

        if(!Util::check_json_validity(db, "live_profiles_scramble", profile, 
                    json::parse( live_profiles_scramble))){
            return;
        } 
        string algorithm_name = profile["offline"]["algorithm"];
        string algorithm_key  = profile["offline"]["key"];
        if(!algorithm_name.empty() && !algorithm_key.empty()){
            gst_mpegts_crypto(in_multicast, INPUT_PORT, out_multicast, channel["decrypt"], 
                    algorithm_name, algorithm_key);
        }else{
            // TODO: support CCCAM 
            LOG(error) << "CCCAM not support, Only support offline scrambling!";
        }
    }catch(std::exception& e){
        LOG(error) << e.what();
    }

}
