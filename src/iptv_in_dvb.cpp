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
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>
#include <thread>
#include <boost/format.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/tokenizer.hpp>
#include "utils.hpp"
#define BY_DVBLAST 1
#define MAX_IN_TUNER 32
#define MAX_OUT_TUNER 32

using namespace std;
using nlohmann::json;

void start_channel(json tuner, live_setting live_config);
void report_tuners(Mongo& db);
std::pair<int,int> extract_dvbs_parameters(string parameters);

/*
 *   the main()
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
    json tuners = json::parse(db.find_mony("live_tuners_info", 
                "{\"active\":true}"));

    LOG(debug) << "Active tuner number:" << tuners.size();
     
    json channels = json::parse(db.find_mony("live_inputs_dvb", 
                    "{\"active\":true}"));
    for(auto& chan : channels ){
        if(Util::chan_in_output(db, chan["_id"], live_config.type_id)){
            for(auto& tuner : tuners ){
                int t_id = tuner["_id"];
                int c_id = chan["dvbId"];
                if(t_id == c_id){
                    if(tuner["channels"].is_null())
                        tuner["channels"] = json::array();
                    tuner["channels"].push_back(chan);
                    LOG(trace) << "Add channel " << chan["_id"] << " to tuner " << t_id;
                    break;
                }
            }
        }
    }
    for(auto& tuner : tuners ){
        if(!tuner["channels"].is_null()){
            string dvb_path = "/dev/dvb/adapter" + to_string(tuner["systemId"]); 
            if(boost::filesystem::exists(dvb_path)){
                pool.emplace_back(start_channel, tuner, live_config);
            }else{
                LOG(error) << "DVB Not found: "<< dvb_path;
            }
        }
    }

    report_tuners(db);
    for(auto& t : pool){
        t.join();
    }
    Util::wait_forever();
} 
/*
 *  The channel thread function
 *  @param tuner : config of tuner
 *  @param live_config : general live streamer config
 *
 * */
void start_channel(json tuner, live_setting live_config)
{
    Mongo db;
    try{

    LOG(trace) << tuner.dump(4);
    json filter;
    filter["active"] = true;
    filter["_id"] = tuner["frequencyId"];
    json frequency = json::parse(db.find_one("live_satellites_frequencies", 
                    filter.dump()));
    if(frequency["_id"].is_null()){
        LOG(error) << "Invalid frequency for tuner " << tuner["_id"];
        return;
    }
    int freq = frequency["frequency"];
    string parameters = frequency["parameters"];
#if BY_DVBLAST
    string fromdvb_args = "";
    if (tuner["dvbt"])   
        fromdvb_args = "-f" + to_string(freq);
    else{
        auto [pol, symbol_rate] = extract_dvbs_parameters(parameters);
        auto args = boost::format("-f%d -s%d -v%d -S%d") 
            % freq 
            % symbol_rate 
            % pol 
            % tuner["diSEqC"].get<int>();
        fromdvb_args = args.str();
    }
    string cfg_name = "/opt/sms/tmp/fromdvb_"+ to_string(tuner["systemId"]);
    ofstream cfg(cfg_name);
    if(!cfg.is_open()) LOG(error) << "Can't open fromdvb config file";
    for(auto& chan : tuner["channels"]){
        json channel = json::parse(db.find_id("live_satellites_channels", 
                    chan["channelId"] ));
        if(channel["_id"].is_null()){
            LOG(error) << "Invalid satellites_channels for chan " 
                <<  chan["_id"];
            continue;
        } 
        auto multicast = Util::get_multicast(live_config, chan["_id"]);

        int sid = channel["serviceId"];
        auto addr = boost::format("%s:%d@127.0.0.1  1   %d\n") 
            % multicast % INPUT_PORT % sid;
        cfg << addr.str(); 
        LOG(info) << "Info:"<< tuner["_id"] 
            << " DVB:" << tuner["systemId"] << " cfg:" 
            << addr.str(); 
    }
    cfg.close();
    auto cmd = boost::format("/opt/sms/bin/fromdvb -WYCUlu -t0 -a%d -c%s %s")
            % tuner["systemId"] % cfg_name % fromdvb_args ; 
    Util::system(cmd.str());
#else
    // TODO: do by Gstreamer
#endif
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
}
/*
 *   Write status of input/output Tuners to DB
 *   
 * */
void report_tuners(Mongo& db)
{
    while(true){
        json input_tuners = json::array();
        for(size_t i=0; i<MAX_IN_TUNER; ++i){
            string file_path = "/tmp/dvbstat_"+to_string(i)+".txt";
            if(boost::filesystem::exists(file_path)){
                json tuner_info = json::parse(db.find_one("live_tuners_info", 
                        "{\"systemId\":" + to_string(i) + "}"));
                if(tuner_info["_id"].is_null()) 
                    continue;
                string content = Util::get_file_content(file_path);
                if(content.size() < 10) 
                    continue;
                LOG(debug) << "Content of " << file_path << " " << content;
                boost::tokenizer<> tok(content);
                // 30176 48544 0 lock 3474 S 1732207376
                auto it = tok.begin();
                int signal = stoi(*(it++));
                int snr = stoi(*(it++));
                json input_tuner;
                input_tuner["tuner"] = tuner_info["_id"];
                input_tuner["systemId"] = i;
                input_tuner["signal"] = signal;
                input_tuner["snr"] = snr;
                input_tuners.push_back(input_tuner);
            }
        }
        json output_tuners = json::array();
        for(size_t i=0; i<MAX_OUT_TUNER; ++i){
            string file_path = "/tmp/padding_" + to_string(i) + ".txt";
            if(boost::filesystem::exists(file_path)){
                /*
                TODO: find systemId in tuner_info > 1000
                json tuner_info = json::parse(db.find_one("live_tuners_info", 
                        "{\"systemId\":" + to_string(i) + "}"));
                if(tuner_info["_id"].is_null()) 
                    continue;
                */
                string content = Util::get_file_content(file_path);
                if(content.size() < 10) 
                    continue;
                LOG(debug) << "Content of " << file_path << " " << content;
                json output_tuner;
                output_tuner["tuner"] = i;
                output_tuner["capacity"] = stoi(content);
                output_tuners.push_back(output_tuner);
            }
        }
        json report_tuners;
        long now = time(nullptr);
        report_tuners["_id"] = now;
        report_tuners["time"] = now;
        report_tuners["systemId"] = Util::get_systemId(db);
        report_tuners["input"] = input_tuners;
        report_tuners["output"] = output_tuners;
        db.insert("report_tuners", report_tuners.dump());
        LOG(info) << "update tuners report";
        Util::wait(1000 * 60);
    }

}
std::pair<int,int> extract_dvbs_parameters(string parameters)
{
    string type, freq, polarization, symbol_rate;
    istringstream in {parameters};
    in >> type;
    if(type != "S" && type != "S2") 
        throw runtime_error("invalid DVBS parameters");
    in >> freq >> polarization >> symbol_rate;
    int sym = stoi(symbol_rate);
    int pol = polarization == "H" ? 18 : 13; 
    return std::make_pair(pol, sym);
}
