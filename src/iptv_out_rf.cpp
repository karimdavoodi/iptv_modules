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
void start_channel(json channel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    LOG(error) << "TODO: implement RF ...";
}
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    map<int, vector<json> > chan_by_freq;
    vector<string> tomts_cmd;
    vector<string> torf_cmd;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "archive")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["freq"].is_null()){
            LOG(info) << chan.dump();
            continue;
        }
        int freq = chan["freq"].get<int>() / 1000;
        if(freq > 0){
            if(chan["inputType"] != live_config.virtual_dvb_id &&
                    chan["inputType"] != live_config.virtual_net_id  )
                chan_by_freq[freq].push_back(chan); 
                //pool.emplace_back(start_channel, chan, live_config);
        }
    }
    int tid = 1, i = 1;
    json live_tuners_output = json::parse(db.find_id("live_tuners_output", 1));
    if(live_tuners_output["_id"].is_null()){
        LOG(error) << "Invalid live_tuners_output!";
        return 0;
    }
    int pcr = live_tuners_output["dvbtPcr"];
    float bandwidth = live_tuners_output["dvbtBandwidth"];
    vector<int> dvbt_freq = live_tuners_output["dvbt"].get<vector<int>>();
    for(const auto& rf : chan_by_freq){
        auto pos = std::find(dvbt_freq.begin(), dvbt_freq.end(), rf.first);
        if(pos == dvbt_freq.end()){
            LOG(error) << "Output Tuner frequency not set " << rf.first;
            continue;
        }
        int tuner_id = pos - dvbt_freq.begin();
        if(!boost::filesystem::exists("/dev/usb-it950x"+to_string(tuner_id))){
            LOG(error) << "Output Tuner not found: " << tuner_id;
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
        for(const auto& chan: rf.second){
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
        int port = 1200 + tid;
        std::ostringstream tomts; 
        tomts << "/opt/sms/bin/tomts -q -c " << tid 
            << " -B " << bandwidth << " -t0 -O 127.0.0.1 "
            << " -m " << pcr << " -P " << port << " &";
        std::ostringstream torf;
        torf << "/opt/sms/bin/torft "
            << tuner_id << " " <<  rf.first << " " << port << " &";
        LOG(info) << tomts.str();
        LOG(info) << torf.str();
        Util::system(tomts.str());
        Util::system(torf.str());
        tid++;
    }
    THE_END;
} 
