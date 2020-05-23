// TODO: implement save stat of players for resume in restart
#include <boost/filesystem/operations.hpp>
#include <boost/format/format_fwd.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <gstreamermm.h>
#include <boost/format.hpp>
#include "utils.hpp"
#define BY_DVBLAST 1
using namespace std;
using nlohmann::json;

void start_channel(json tuner, live_setting live_config)
{
    BOOST_LOG_TRIVIAL(trace) << tuner.dump(4);
    string fromdvb_args = "";
    if (tuner["is_dvbt"])   
        fromdvb_args = "-f" + to_string(tuner["freq"]) + "000";
    else{
        int pol = tuner["pol"] == "H" ? 18 : 13;
        auto args = boost::format("-f%d000 -s%d000 -v%d -S%d") 
            % tuner["freq"] % tuner["symb"] % pol % tuner["switch"];
        fromdvb_args = args.str();
    }
    string cfg_name = "/opt/sms/tmp/fromdvb_"+ to_string(tuner["_id"]);
    ofstream cfg(cfg_name);
    for(auto& chan : tuner["channels"]){
        auto multicast = get_multicast(live_config, chan["_id"]);
        auto addr = boost::format("%s:%d@127.0.0.1  1   %d\n") 
            % multicast % INPUT_PORT % chan["sid"];
        cfg << addr.str(); 
        BOOST_LOG_TRIVIAL(info) << "DVB:" << tuner["_id"] << " chan:" 
            << chan["name"]  << " -> " << multicast; 
    }
    cfg.close();
#if BY_DVBLAST
    auto cmd = boost::format("/opt/sms/bin/fromdvb -WYCUlu -t0 -a%d -c%s %s")
            % tuner["_id"] % cfg_name % fromdvb_args ; 
    string cmd_str = cmd.str();
    BOOST_LOG_TRIVIAL(info) <<  cmd_str;
    system(cmd_str.c_str());
#else
#endif
}
int main()
{
    vector<thread> pool;
    live_setting live_config;

    init();
    if(!get_live_config(live_config, "dvb")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    json tuners = json::parse(Mongo::find("live_tuners_input", "{}"));
    json silver_channels = json::parse(Mongo::find("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        if(chan["inputType"] == live_config.type_id){
            json dvb_chan = json::parse(Mongo::find_id("live_inputs_dvb", chan["inputId"]));
            IS_CHANNEL_VALID(dvb_chan);
            for(auto& tuner : tuners ){
                int t_id = tuner["_id"];
                int c_id = dvb_chan["dvb_id"];
                if(t_id == c_id){
                    IS_CHANNEL_VALID(tuner);
                    if(tuner["channels"].is_null())
                        tuner["channels"] = json::array();
                    tuner["channels"].push_back(dvb_chan);
                    break;
                }
            }
        }
    }
    for(auto& tuner : tuners ){
        if(!tuner["channels"].is_null()){
            string dvb_path = "/dev/dvb/adapter" + to_string(tuner["_id"]); 
            if(boost::filesystem::exists(dvb_path)){
                pool.emplace_back(start_channel, tuner, live_config);
            }else{
                BOOST_LOG_TRIVIAL(error) << "DVB Not found: "<< dvb_path;
            }
        }
    }
    for(auto& t : pool){
        t.join();
    }
    BOOST_LOG_TRIVIAL(info) << "End!";
    return 0;
} 
