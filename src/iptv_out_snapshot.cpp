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
#include <boost/filesystem/operations.hpp>
#include <chrono>
#include <ctime>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"

#define SNAPSHOT_PERIOD (60*10)

using namespace std;

bool gst_capture_udp_in_jpg(string in_multicast, int port, string pic_path);
void start_snapshot(const json& channel, live_setting live_config);

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
    if(!Util::get_live_config(db, live_config, "dvb")){
        LOG(error) << "Error in live config! Exit.";
        return -1;
    }
    Util::check_path(string(MEDIA_ROOT) + "Snapshot");

    json channels_net = json::parse(db.find_mony("live_output_network", 
                "{\"active\":true}"));
    json channels_dvb = json::parse(db.find_mony("live_output_dvb", 
                "{\"active\":true}"));
    for(auto& chan : channels_net ){
        chan["name"] = Util::get_channel_name(chan["input"], chan["inputType"]);
    }
    for(auto& chan : channels_dvb ){
        chan["name"] = Util::get_channel_name(chan["input"], chan["inputType"]);
    }
    while(true){
        auto start = time(nullptr);
        for(auto& chan : channels_net ){
            start_snapshot(chan, live_config);
        }
        for(auto& chan : channels_dvb ){
            start_snapshot(chan, live_config);
        }
        auto duration = time(nullptr) - start;
        if(duration < SNAPSHOT_PERIOD){
            LOG(info) << "Wait for next snapshot for " << SNAPSHOT_PERIOD - duration;
            Util::wait((SNAPSHOT_PERIOD - duration) * 1000);
        } 
    }
    Util::wait_forever();
} 
/*
 *  The channel thread function
 *
 *  @param channel : config of channel
 *  @param live_config : general live streamer config
 *
 * */
void start_snapshot(const json& channel, live_setting live_config)
{
    Mongo db;
    auto now = time(nullptr);
    auto now_tm = localtime(&now);
    live_config.type_id = channel["inputType"].get<int>();
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    long channel_id = channel["_id"].get<long>();
    auto pic_path = boost::format("%sSnapshot/%ld.jpg")
        % MEDIA_ROOT % channel_id; 

    if(boost::filesystem::exists(pic_path.str())){
        boost::filesystem::remove(pic_path.str());
        LOG(info) << "Delete old snapshut:" << pic_path.str();
    }

    LOG(debug) << "Try to snapsot from " << channel["name"];
    bool succesfull = gst_capture_udp_in_jpg(in_multicast, INPUT_PORT, pic_path.str());

    if(succesfull){
        string name = channel["name"].get<string>();
        LOG(info) << "Capture " << name << " in " << pic_path.str();
        json media = json::object();
        media["_id"] = channel_id;
        media["name"] = name;
        media["format"] = CONTENT_FORMAT_JPG; 
        media["type"] = CONTENT_TYPE_SNAPSHOT;
        media["price"] = 0;
        media["date"] = now_tm->tm_year;
        media["languages"] = json::array();
        media["platform"] = json::array();
        media["category"] = channel["category"];
        media["description"] = {
            {"en",{
                      { "name" ,name },
                      { "description" ,"" }
                  }},
            {"fa",{
                      { "name" ,name },
                      { "description" ,"" }
                  }},
            {"ar",{
                      { "name" ,name },
                      { "description" ,"" }
                  }}
        };
        db.insert_or_replace_id("storage_contents_info", channel_id, media.dump());
    }
    json report = json::object();
    report["_id"] = std::chrono::system_clock::now().time_since_epoch().count();
    report["time"] = time(nullptr);
    report["systemId"] = Util::get_systemId(db);
    report["inputId"] = channel["input"];
    report["inputType"] = channel["inputType"];
    report["status"] = succesfull ? 100 : 0;
    report["snapshot"] = succesfull ?  channel_id : 0;
    db.insert("report_channels", report.dump());
    // Update report_output_channels
    json report_output_channel = json::parse(db.find_id("report_output_channels",channel_id));
    if(!report_output_channel["name"].is_null()){
        report_output_channel["inputStatus"] =  report["status"];
        report_output_channel["inputSnapshot"] =  report["snapshot"];
        db.insert_or_replace_id("report_output_channels", channel_id, 
                report_output_channel.dump());
    }
    Util::wait(1000);
}
