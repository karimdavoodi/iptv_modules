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
#include <exception>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <map>
#include <thread>
#include <set>
#include <boost/filesystem/operations.hpp>
#include "utils.hpp"
#include "db_structure.hpp"
using namespace std;
void generate_serviceId(Mongo&);

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
    generate_serviceId(db);
    json channels = json::parse(db.find_mony("live_output_dvb", R"({"active":true})")); 

    for(auto& chan : channels ){
        if(!Util::check_json_validity(db, "live_output_dvb", chan, 
                json::parse( live_output_dvb))) 
            continue;
        int dvbId = chan["dvbId"];
        chan_by_dvbId[dvbId].push_back(chan); 
    }
    LOG(debug) << "Dvb map size " << chan_by_dvbId.size();
    auto epg_file = "/tmp/mts_epg.conf";
    ofstream epg(epg_file);
    if(epg.is_open()){
        epg << endl;
        epg.close();
    }
    int tid = 1;
    for(auto& iter : chan_by_dvbId){
        
        int dvbId = iter.first;
        vector<json>& chans = iter.second;

        LOG(debug) << "DVB " << dvbId << " Chan number " << chans.size();
        if(chans.empty()){
            DB_ERROR(db, 2) << "Not found channel for tuner id " << dvbId;
            continue;
        }    
        json tuner = json::parse(db.find_id("live_tuners_info", dvbId));
        if(tuner["_id"].is_null() || tuner["frequencyId"].is_null()){
            DB_ERROR(db, 2) << "Tuner not found " << dvbId;
            continue;
        }
        if(tuner["active"] == false || tuner["frequencyId"].is_null()){
            DB_ERROR(db, 2) << "Tuner is inactive or invalid " << dvbId;
            continue;
        }
        json frequency = json::parse(db.find_id("live_satellites_frequencies", 
                    tuner["frequencyId"]));
        if(frequency["_id"].is_null()){
            DB_ERROR(db, 2) << "Frequency not found " << tuner["frequencyId"].get<int64_t>();
            continue;
        }
        int systemId = tuner["systemId"];
        string tuner_cmd;
        string tuner_path;
        if(systemId >= 2000){
            systemId -= 2000;
            tuner_path = "/dev/usb-it950x" + to_string(systemId);
            tuner_cmd  = "/opt/sms/bin/torft_it950";
        }else if(systemId >= 1000){
            systemId -= 1000;
            auto dev_major = to_string(systemId / 100);
            auto dev_minor = to_string(systemId % 10 );
            tuner_path = "/dev/tbsmod" + dev_major + "/mod" + dev_minor;
            tuner_cmd  = "/opt/sms/bin/torft_tbs";
        }else{
            DB_ERROR(db, 2) << "invalid system tuner id " << systemId;
            continue;
        }
        if(!boost::filesystem::exists(tuner_path)){
            DB_ERROR(db, 2) << "Output Tuner not found: " << tuner_path;
            continue;
        }
        auto cfg_file = "/tmp/mts_chan"+to_string(tid)+".conf";
        ofstream cfg(cfg_file);
        if(!cfg.is_open()){
            DB_ERROR(db, 2) << "Can't open tomts cfg file! " << cfg_file;
            continue;
        }
        LOG(debug) << "Fill  " << cfg_file;
        cfg << "[Global]\n"
            "provider_name = MoojAfzar\n"
            "transport_stream_id = "  << tid << "\n" << endl;
        int channel_number = 0;
        for(auto& chan: chans){
            if(chan["input"].is_null() || 
                    chan["inputType"].is_null() || 
                    chan["serviceId"].is_null()){
                DB_ERROR(db, 2) << "Invalid channel in " << dvbId << ":" << chan["_id"].get<int64_t>();
                continue;
            }
            auto chan_name = Util::get_channel_name(chan["input"], chan["inputType"]);
            live_config.type_id = chan["inputType"];
            auto in_multicast = Util::get_multicast(live_config, chan["input"]);
            cfg << "\n[Channel" << channel_number + 1 << "]"
                << "\nservice_id = " << chan["serviceId"] 
                << "\nid = "<< chan["serviceId"] 
                << "\nname = " << chan_name 
                << "\nradio = no"
                << "\nlive = 1"
                << "\nsource = udp://" << in_multicast <<  ":" << INPUT_PORT << endl; 
            channel_number++; 
            LOG(debug) << "Add " << chan_name 
                << " sid:" << chan["serviceId"] << " to " << cfg_file;
        }
        cfg.flush();
        cfg.close();

        if(channel_number == 0){
            LOG(warning) << "Not add channel to " << dvbId;
            continue;
        }
        
        int freq = frequency["frequency"];
        float bandwidth = 31.7; // TODO : calc from frequency["parameters"]
        int pcr = 0;

        int port = 1200 + tid;
        std::ostringstream tomts; 
        tomts << "/opt/sms/bin/tomts -q -c " << cfg_file 
            << " -B " << bandwidth << " -t0 -O 127.0.0.1 "
            << " -m " << pcr << " -P " << port << " &";
        std::ostringstream torf;
        torf << tuner_cmd << " "
            << systemId << " " << freq/1000 << " " << port << " &";
        Util::system(tomts.str());
        Util::system(torf.str());
        tid++;
    }
    Util::wait_forever();
} 
void generate_serviceId(Mongo& db)
{
    set<long> service_ids;
    long service_id = 101;
    try{
        json channels = json::parse(db.find_mony("live_output_dvb", R"({"active":true})")); 
        for(const auto& chan : channels){
            if(chan["serviceId"].is_number()){
                service_ids.insert(chan["serviceId"].get<long>());
            }
        }
        for(auto& chan : channels){
            if(chan["serviceId"].is_null() || !chan["serviceId"].is_number()){
                while(service_ids.count(service_id)) 
                    service_id++;
                chan["serviceId"] = service_id;
                db.replace_id("live_output_dvb", chan["_id"], chan.dump());
                LOG(info) << "Generate serviceId " << service_id << 
                    " for channel " << chan["_id"];
                service_id++;
            }
        }
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
}
