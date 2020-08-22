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
#include <fstream>
#include <vector>
#include <sstream>
#include <map>
#include <thread>
#include <boost/filesystem/operations.hpp>
#include "utils.hpp"
using namespace std;
void gst_task(string in_multicast, string out_multicast);
void start_channel(json channel, live_setting live_config);

int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    map<int, vector<json> > chan_by_dvbId;
    vector<string> tomts_cmd;
    vector<string> torf_cmd;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "archive")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    json channels = json::parse(db.find_mony("live_output_dvb", "{\"active\":true}"));
    for(auto& chan : channels ){
        IS_CHANNEL_VALID(chan);
        int dvbId = chan["dvbId"];
        chan_by_dvbId[dvbId].push_back(chan); 
    }
    int tid = 1, i = 1;
    for(const auto& [dvbId, chans] : chan_by_dvbId){
        json tuner = json::parse(db.find_id("live_tuners_info", dvbId));
        if(tuner["_id"].is_null()) continue;
        json frequency = json::parse(db.find_id("live_satellites_frequencies", 
                    tuner["frequencyId"]));
        if(frequency["_id"].is_null()) continue;
        int systemId = tuner["systemId"];
        // TODO: work only with it950
        if(!boost::filesystem::exists("/dev/usb-it950x"+to_string(systemId))){
            LOG(error) << "Output Tuner not found: " << systemId;
            continue;
        }
        ofstream cfg("/opt/sms/tmp/mts_chan"+to_string(tid)+".conf");
        if(!cfg.is_open()){
            LOG(error) << "Can't open tomts cfg file!";
            continue;
        }
        cfg << "[Global]\n"
            "provider_name = MoojAfzar\n"
            "transport_stream_id = "  << tid << "\n\n";
        i = 1;
        for(const auto& chan: chans){
            live_config.type_id = chan["inputType"];
            auto in_multicast = Util::get_multicast(live_config, chan["input"]);
            cfg << "\n[Channel" << i << "]\n"
                << "\nservice_id = " << chan["sid"] 
                << "\nid = "<< chan["sid"] 
                << "\nname = " << chan["name"] 
                << "\nradio = no"
                << "\nlive = 1"
                << "\nsource = udp://" << in_multicast <<  ":" << INPUT_PORT << '\n'; 
            cfg.close();
            i++;
        }
        
        int freq = frequency["frequency"];
        float bandwidth = 31.7; // TODO : calc from frequency["parameters"]
        int pcr = 0;

        int port = 1200 + tid;
        std::ostringstream tomts; 
        tomts << "/opt/sms/bin/tomts -q -c " << tid 
            << " -B " << bandwidth << " -t0 -O 127.0.0.1 "
            << " -m " << pcr << " -P " << port << " &";
        std::ostringstream torf;
        torf << "/opt/sms/bin/torft "
            << systemId << " " << freq << " " << port << " &";
        LOG(info) << tomts.str();
        LOG(info) << torf.str();
        Util::system(tomts.str());
        Util::system(torf.str());
        tid++;
    }
    THE_END;
} 
void start_channel(json channel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    LOG(error) << "TODO: implement RF ...";
}
