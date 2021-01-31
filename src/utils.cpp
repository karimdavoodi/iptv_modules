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
#include <boost/log/core/record_view.hpp>
#include <exception>    
#include <iostream>
#include <string>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/current_process_name.hpp>
#include "utils.hpp"
#include "gst.hpp"
using namespace std;
namespace Util {
    static int systemId = 0;
    int get_systemId(Mongo& db)
    {
        try{
            if(systemId > 0) return systemId;
            json license = json::parse(db.find_id("system_license", 1));
            if(license["license"].is_null()) return 0;
            if(license["license"]["General"]["MMK_ID"].is_null()) return 0;
            if(license["license"]["General"]["MMK_ID"].is_number()){
                systemId = license["license"]["General"]["MMK_ID"];
            }else{
                systemId = std::stoi(license["license"]["General"]["MMK_ID"].get<std::string>());
            }
            return systemId;
        }catch(std::exception& e){
            LOG(error) << e.what();
            return 0;
        }
    }
 
    void system(const std::string cmd)
    {
        LOG(debug) << "Run shell command:" << cmd;
        if(std::system(cmd.c_str())){
            LOG(error) << "Error in run " << cmd;
        }
    }
    void wait(int millisecond)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(millisecond));
    }
    void wait_forever()
    {
        LOG(warning) << "Wait Forever!";
        while(true){
            wait(1000000000L);
        }
    }
    const std::string shell_out(const std::string cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) return "";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }
    void exec_shell_loop(const std::string cmd)
    {
        while(true){
            system(cmd);
            wait(5000);
            LOG(error) << "Re Run: "<<  cmd;
        }
    }
    void boost_log_init(Mongo& db)
    {
        try{
            namespace logging = boost::log;
            namespace keywords = logging::keywords;
            namespace attrs = logging::attributes;
            logging::add_common_attributes();
            logging::core::get()->add_global_attribute("Process", 
                    attrs::current_process_name());

            // Set Debug level
            json system_location = json::parse(db.find_id("system_general",1));
            int debug_level = (!system_location["debug"].is_null())? 
                system_location["debug"].get<int>():5;
            // Check evnironment debug variable
            char* env_iptv_debug_level = getenv("IPTV_DEBUG_LEVEL");
            if(env_iptv_debug_level != nullptr){
                debug_level = atoi(env_iptv_debug_level);
            }
            string out_file = "/tmp/iptv_modules.log"; 
            // Check evnironment debug file
            char* env_iptv_debug_file = getenv("IPTV_DEBUG_FILE");
            if(env_iptv_debug_file != nullptr){
                out_file = env_iptv_debug_file; 
            }
            LOG(info) << "Log file:" << out_file << " level:" << debug_level;
            debug_level = abs(5-debug_level);
            // Set Debug 
            logging::add_file_log
                (
                 keywords::file_name = out_file,
                 keywords::format = "%TimeStamp% %Process% %ThreadID% %Severity%: %Message%",
                 keywords::auto_flush = true,
                 keywords::open_mode = std::ios_base::app
                 //%TimeStamp% %Process% %ThreadID% %Severity% %LineID% %Message%"     
                );
            logging::core::get()->set_filter(
                    logging::trivial::severity >= debug_level);
        }catch(std::exception& e){
            LOG(error) << e.what();
        }
    }
    void _set_gst_debug_level()
    {
        char* d_level = getenv("GST_DEBUG");
        string debug_level = (d_level != nullptr) ? d_level : "";
        if(debug_level  == "WARNING"){
            gst_debug_set_default_threshold(GST_LEVEL_WARNING);
        }else if(debug_level  == "ERROR"){
            gst_debug_set_default_threshold(GST_LEVEL_ERROR);
        }else if(debug_level  == "DEBUG"){
            gst_debug_set_default_threshold(GST_LEVEL_DEBUG);
        }else if(debug_level  == "LOG"){
            gst_debug_set_default_threshold(GST_LEVEL_LOG);
        }else if(debug_level  == "INFO"){
            gst_debug_set_default_threshold(GST_LEVEL_INFO);
        }else if(debug_level  == "TRACE"){
            gst_debug_set_default_threshold(GST_LEVEL_TRACE);
        }
    }
    void _set_internal_multicat_route()
    {
        if( geteuid() == 0 ){
            bool found = false;
            ifstream route("/proc/net/route");
            string line;
            while(route.good()){
                std::getline(route, line);
                if(line.find("lo") != string::npos &&
                        line.find("000000E5")/*229.0.0.0*/ != string::npos ){
                    found = true;
                }
            }
            if(!found)
                add_route_by_mask8(INPUT_MULTICAST, "lo");
            else
                LOG(debug) << "Found local multicast route, not add it";
        }
    }
    void _set_max_open_file(int max)
    {
        struct rlimit rlp;
        rlp.rlim_cur = max;
        rlp.rlim_max = max;
        if(setrlimit(RLIMIT_NOFILE, &rlp)){
            LOG(error) << "Not set max open file:" << strerror(errno);
        }
    }
    void init(Mongo& db)
    {
        try{
            guint major, minor, micro, nano;
            boost_log_init(db);
            gst_init(nullptr, nullptr);
            gst_version (&major, &minor, &micro, &nano);
            LOG(debug) << "Gstreamer version:" 
                << major << "." << minor << "." 
                << micro << "." << nano; 
            _set_gst_debug_level();
            _set_internal_multicat_route();
            _set_max_open_file(50000);
        }catch(std::exception& e){
            LOG(error) << e.what();
        }
    }
    void add_route_by_mask8(int multicast_class, string nic)
    {
        string multicat_addr = to_string(multicast_class) + ".0.0.0";
        string netmask = "255.0.0.0";
        string cmd = "ip route add " + multicat_addr + "/8 dev " + nic;
        system(cmd);
    }
    bool get_live_config(Mongo& db, live_setting& cfg, string type)
    {
        try{
            // Get type id
            json input_type = json::parse(db.find_one("live_inputs_types",
                        "{\"name\":\"" + type + "\"}"));

            if(!input_type.is_null()){
                cfg.type_id =  input_type["_id"];
            }
            // Get network config
            json net = json::parse(db.find_id("system_network",1));
            cfg.multicast_class = net["multicastBase"];
            int m_id = net["multicastInterface"]; 
            int main_id = net["mainInterface"]; 
            for(auto& iface : net["interfaces"]){
                if(iface["_id"] == m_id){
                    cfg.multicast_iface = iface["name"];
                }
                if(iface["_id"] == main_id){
                    cfg.main_iface = iface["name"];
                }
            } 
            if(cfg.multicast_class > 239 || cfg.multicast_class < 224){
                cfg.multicast_class = 239;
                LOG(error) << "Invalid multicat, fix by 239.";
            }
            LOG(debug) 
                << "Live config:  "
                << " multicast_class:" << cfg.multicast_class 
                << " multicast_iface:" << cfg.multicast_iface 
                << " type_id:" << cfg.type_id;
            if(cfg.type_id < 1 || 
                    cfg.multicast_iface.size() < 1){
                DB_ERROR(db, 1) << "Invalid system_network!";
                return false;
            } 
            return true;
        }catch(std::exception& e){
            LOG(error) << e.what();
            return false;
        }
    }
    const string get_multicast(const live_setting& config, int channel_id, bool out_multicast)  
    {
        uint32_t address = 0;
        uint8_t channel_id_byte1 = (channel_id & 0x000000ff);  
        uint8_t channel_id_byte2 = (channel_id & 0x0000ff00) >> 8;  
        address  = out_multicast ? config.multicast_class : INPUT_MULTICAST ;
        if(address < 224 || address > 239) address = 239;
        address += config.type_id << 8;
        address += channel_id_byte2 << 16;
        address += channel_id_byte1 << 24;
        struct in_addr addr;
        addr.s_addr = address;
        string addr_str =  inet_ntoa(addr);
        LOG(debug) 
            << " multicast_class:" << config.multicast_class 
            << " type_id:" << config.type_id 
            << " channel_id: " << channel_id
            << " --> " << addr_str;
        return addr_str;
    }
    const string get_content_path(Mongo& db, int id)
    {
        try{
            json content_info = json::parse(db.find_id("storage_contents_info",id));
            if(content_info["type"].is_null()){
                LOG(warning) << "Invalid content info by id " << id;
                return "";
            }
            json content_type = json::parse(db.find_id("storage_contents_types",
                        content_info["type"]));
            json content_format = json::parse(db.find_id("storage_contents_formats",
                        content_info["format"]));
            if(content_type["name"].is_null() || content_format["name"].is_null()){
                LOG(error) << "Invalid contents 'type' or 'format'";
                return "";
            }
            string path = string(MEDIA_ROOT);
            path += content_type["name"];
            path +=  "/";
            path += to_string(id);
            path += ".";
            path += content_format["name"];
            LOG(trace) << "Media Path:" << path;
            return path;
        }catch(std::exception& e){
            LOG(error) << e.what();
            return "";
        }
    }
    void check_path(const std::string path)
    {
        if(!boost::filesystem::exists(path)){
            LOG(info) << "Create " << path;
            boost::filesystem::create_directories(path);
        }
    }
    const std::string get_file_content(const std::string name)
    {
        if(boost::filesystem::exists(name)){
            ifstream file(name);
            if(file.is_open()){
                std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
                return content;
            }
        }
        LOG(warning)  <<  "file not exists: " << name;
        return "";
    }
    const pair<int,int> profile_resolution_pair(const string p_vsize)
    {
        if(p_vsize.find("SD") != string::npos)        return make_pair(720, 576);
        else if(p_vsize.find("FHD") != string::npos)  return make_pair(1920, 1080);
        else if(p_vsize.find("4K") != string::npos)   return make_pair(4096,2048);
        else if(p_vsize.find("HD") != string::npos)   return make_pair(1280, 720);
        else if(p_vsize.find("CD") != string::npos)   return make_pair(320, 240);
        return make_pair(0, 0);
    }
    const string profile_resolution(const string p_vsize)
    {
        if(p_vsize.find("SD") != string::npos)        return "720x576";
        else if(p_vsize.find("FHD") != string::npos)  return "1920x1080";
        else if(p_vsize.find("4K") != string::npos)   return "4096x2048";
        else if(p_vsize.find("HD") != string::npos)   return "1280x720";
        else if(p_vsize.find("CD") != string::npos)   return "320x240";
        return "";
    }
    bool check_weektime(Mongo& db, int weektime_id)
    {
        // TODO: implement
        json weektime = json::parse(db.find_id("system_weektime", weektime_id));
        if(weektime["active"].is_null() || weektime["active"] == false){
            LOG(error) << "Invalid  or inactive weektime " << weektime_id;
            return false;
        }

        time_t now = time(nullptr);
        auto now_tm = localtime(&now);
        auto now_wday = to_string((now_tm->tm_wday + 1) % 7);
        auto hours = weektime["hours"][now_wday].get<vector<int>>(); 
        for(int h : hours ){
            if(h == now_tm->tm_hour) return true;
        }
        return false;
    }
    bool chan_in_input(Mongo &db, int input, int input_type)
    {
        try{
            json type = json::parse(db.find_id("live_inputs_types", input_type));
            if(type.is_null()){
                DB_ERROR(db, 2) << "In the live_inputs_types not found inputType:" << input_type;
                return false;
            } 
            string input_col = "live_inputs_" + type["name"].get<string>();

            json filter;
            filter["active"] = true;
            filter["_id"] = input;
            if(db.count(input_col, filter.dump()) == 0){
                DB_ERROR(db, 2) << "Not found ACTIVE channel id:" << input 
                    << " in " << input_col; 
                return false;
            }
            return true;
        }catch(std::exception& e){
            LOG(error)  <<  e.what();
        }
        return false;
    }
    bool chan_in_output(Mongo &db, int chan_id, int chan_type)
    {
        try{
            json filter;
            filter["active"] = true;
            filter["input"] = chan_id;
            filter["inputType"] = chan_type;

            json out_network = json::parse(db.find_mony("live_output_network", filter.dump()));
            for(auto& chan : out_network){
                if(chan["udp"] || chan["http"] || chan["rtsp"] || chan["hls"])
                    return true;
            }
            int out_dvb = db.count("live_output_dvb", filter.dump());
            if(out_dvb > 0)
                return true;

            json out_hdd = json::parse(db.find_mony("live_output_archive", filter.dump()));
            for(auto& chan : out_hdd){
                if(chan["timeShift"] > 0  || chan["programName"].size() > 0 )
                    return true;
            }
            if(chan_type <= 3 ){
                // Check processed inputs as dest of stream
                if (db.count("live_inputs_transcode", filter.dump()) > 0)
                    return true;
                if (db.count("live_inputs_scramble", filter.dump()) > 0)
                    return true;

                json filter_mix1;
                filter_mix1["active"] = true;
                filter_mix1["input1"] = chan_id;
                filter_mix1["inputType1"] = chan_type;
                if (db.count("live_inputs_mix", filter_mix1.dump()) > 0)
                    return true;
                json filter_mix2;
                filter_mix2["active"] = true;
                filter_mix2["input2"] = chan_id;
                filter_mix2["inputType2"] = chan_type;
                if (db.count("live_inputs_mix", filter_mix2.dump()) > 0)
                    return true;
            }
        }catch(std::exception const& e){
            LOG(error)  <<  e.what();
        }
        LOG(info) << "The channel id:" << chan_id << " not used!"; 
        return false;
    }
    bool is_channel_tv(int64_t input_id, int input_type)
    {   
        try{
            Mongo db;
            json type_name = json::parse(db.find_id("live_inputs_types",input_type)); 
            if(type_name["name"].is_null()){
                DB_ERROR(db, 2) << "In the live_inputs_types not found inputType:" << input_type;
                return false;
            }
            string input_rec_name = "live_inputs_" + type_name["name"].get<string>();
            json input_chan = json::parse(db.find_id(input_rec_name, input_id)); 
            if(input_chan["tv"].is_boolean()){
                return input_chan["tv"];
            }
        }catch(std::exception const& e){
            LOG(error)  <<  e.what();
        }
        return false;
    }
    const std::string get_channel_name(int64_t input_id, int input_type)
    {   
        try{
            Mongo db;
            json type_name = json::parse(db.find_id("live_inputs_types",input_type)); 
            if(type_name["name"].is_null()){
                DB_ERROR(db, 2) << "In the live_inputs_types not found inputType:" << input_type;
                return "";
            }
            string input_rec_name = "live_inputs_" + type_name["name"].get<string>();
            json input_chan = json::parse(db.find_id(input_rec_name, input_id)); 
            if(input_chan["_id"].is_null()){
                DB_ERROR(db, 2) << "Invalid input chan by id  " << input_id 
                    << " in " << type_name["name"].get<string>();
                return "";
            }
            if(input_chan["name"].is_string()){
                return input_chan["name"];
            }
            // try to find 'name' from input channels
            string input_type_name = type_name["name"];
            if(input_type_name == "dvb"){
                json channel = json::parse(db.find_id("live_satellites_channels", 
                            input_chan["channelId"])); 
                if(channel["name"].is_string()) 
                    return channel["name"];
            }
            else if(input_type_name == "network"){
                json channel = json::parse(db.find_id("live_network_channels", 
                            input_chan["channelId"])); 
                if(channel["name"].is_string()) 
                    return channel["name"];
            }
        }catch(std::exception const& e){
            LOG(error)  <<  e.what();
        }
        return "";
    }
    bool check_json_validity(Mongo& db, const string record_name,json& record, const json target)
    {
        bool result = true;
        try{
            // TODO: chack for null value
            for(auto& [key, value]: target.items()){
                if(!key.size()) continue;
                if(value.is_array()){
                    result = result && check_json_validity(db, record_name,record[key][0], value[0]);
                } else if(value.is_structured()){
                    result = result && check_json_validity(db, record_name,record[key], value);
                } else if(record.is_null() || !record.contains(key)){
                    if(key == "description" || 
                            key == "programName" ||     // in output/archive
                            key == "diSEqC" ||          // in tuner info for dvbt
                            key == "extra" ||           // in profile/transcode
                            key == "tv" || 
                            key == "logo"){
                        LOG(warning) << "In " << record_name << " not found the key:" << key;
                        result = result && true; // TODO: Ignore this fields
                    }else{
                        DB_ERROR(db, 1) << "Problem in " << record_name << " with " << key;
                        result = result && false;
                    }
                }
            }
        }catch(std::exception const& e){
            LOG(error)  <<  e.what();
            return false;
        }
        return result;
    }
    void create_directory(const std::string path)
    {
        try{
            if(!boost::filesystem::exists(path)){
                LOG(info) << "Create " << path;
                boost::filesystem::create_directory(path);
            }
        }catch(std::exception const& e){
            LOG(error)  <<  e.what();
        }

    }
}
void Error::report_error()
{
    try{
        if(auto pos = file.find_last_of("/"); pos != string::npos) 
            file = file.substr(pos+1);
        if(auto pos = file.find_last_of("."); pos != string::npos) 
            file = file.substr(0, pos);

        BOOST_LOG_TRIVIAL(error) << "\033[0;32m[" << file << ":" << func << ":" << line 
            << "]\033[0m " << message;

        json j = json::object();
        j["_id"] = chrono::system_clock::now().time_since_epoch().count();
        j["time"] = long(time(nullptr));
        j["process"] = file;
        j["message"] = message;
        j["level"] = level;
        db.insert("report_error", j.dump());

    }catch(std::exception const& e){
        //LOG(error)  <<  e.what();
    }
}
