#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <thread>
#include "utils.hpp"
using namespace std;
void gst_task(string in_multicast, string out_multicast);

void start_channel(json channel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = get_multicast(live_config, channel["inputId"]);
    BOOST_LOG_TRIVIAL(error) << "TODO: implement RF ...";
}
int main()
{
    vector<thread> pool;
    live_setting live_config;
    map<int, vector<json> > chan_by_freq;
    vector<string> tomts_cmd;
    vector<string> torf_cmd;

    init();
    if(!get_live_config(live_config, "archive")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(Mongo::find("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["freq"].is_null()){
            BOOST_LOG_TRIVIAL(info) << chan.dump();
            continue;
        }
        int freq = chan["freq"];
        if(freq > 0){
            if(chan["inputType"] != live_config.virtual_dvb_id &&
                    chan["inputType"] != live_config.virtual_net_id  )
                chan_by_freq[freq].push_back(chan); 
                //pool.emplace_back(start_channel, chan, live_config);
        }
    }
    int tid = 1, i = 1;
    json live_tuners_output = json::parse(Mongo::find_id("live_tuners_output", 1));
    if(live_tuners_output["_id"].is_null()){
        BOOST_LOG_TRIVIAL(error) << "Invalid live_tuners_output!";
        return 0;
    }
    int pcr = live_tuners_output["dvbtPcr"];
    float bandwidth = live_tuners_output["dvbtBandwidth"];
    for(const auto& rf : chan_by_freq){
        ofstream cfg("/opt/sms/tmp/mts_chan"+to_string(tid)+".conf");
        if(!cfg.is_open()){
            BOOST_LOG_TRIVIAL(error) << "Can't open tomts cfg file!";
            continue;
        }
        cfg << "[Global]\n"
            "provider_name = MoojAfzar\n"
            "transport_stream_id = "  << tid << "\n\n";
        i = 1;
        for(const auto& chan: rf.second){
            live_config.type_id = chan["inputType"];
            auto in_multicast = get_multicast(live_config, chan["inputId"]);
            cfg << "\n[Channel" << i << "]\n"
                << "\nservice_id = " << chan["sid"] 
                << "\nid = "<< chan["sid"] 
                << "\nname = " << chan["name"] 
                << "\nradio = no"
                << "\nlive = 1"
                << "\nsource = udp://" << in_multicast <<  ":" << INPUT_PORT << '\n'; 
            cfg.close();
            int port = 1200 + tid;
            std::ostringstream tomts; 
            tomts << "/opt/sms/bin/tomts -q -c " << tid 
                << "-B " << bandwidth << " -t0 -O 127.0.0.1 "
                << " -m " << pcr << " -P " << port << " &";
            std::ostringstream torf;
            torf << "/opt/sms/bin/torft "
                << tid << " " <<  rf.first << " " << port << " &";

            BOOST_LOG_TRIVIAL(info) << tomts.str();
            BOOST_LOG_TRIVIAL(info) << torf.str();
            std::system(tomts.str().c_str());
            std::system(torf.str().c_str());
        }
    }
    while(true)
        std::this_thread::sleep_for(std::chrono::seconds(100));
    return 0;
} 
