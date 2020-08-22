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
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/current_process_name.hpp>
#include "utils.hpp"
#include "gst.hpp"
using namespace std;
namespace Util {
    int systemId = 0;
    int get_systemId(Mongo& db)
    {
        try{
            if(systemId > 0) return systemId;
            json license = json::parse(db.find_id("system_license", 1));
            if(license["license"].is_null()) return 0;
            if(license["license"]["General"]["MMK_ID"].is_null()) return 0;
            systemId = license["license"]["General"]["MMK_ID"];
            return systemId;
        }catch( ... ){
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
            logging::core::get()->add_global_attribute("Process", attrs::current_process_name());

            // Set Debug level
            json system_location = json::parse(db.find_id("system_general",1));
            int debug_level = (!system_location["debug"].is_null())? 
                system_location["debug"].get<int>():5;
            // Check evnironment debug variable
            char* env_iptv_debug_level = getenv("IPTV_DEBUG_LEVEL");
            if(env_iptv_debug_level != nullptr){
                debug_level = atoi(env_iptv_debug_level);
            }
            string out_file = "/opt/sms/tmp/log.log"; 
            char* env_iptv_debug_file = getenv("IPTV_DEBUG_FILE");
            if(env_iptv_debug_file != nullptr){
                out_file = env_iptv_debug_file; 
            }
            LOG(info) << "Log file:" << out_file << " level:" << debug_level;
            debug_level = abs(5-debug_level);

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
            LOG(error) << "Exception " << e.what();
        }
    }
    void init(Mongo& db)
    {
        try{
            boost_log_init(db);
            gst_init(nullptr, nullptr);
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
            // Add internal multicast net to localhost 
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
                    route_add(INPUT_MULTICAST, "lo");
                else
                    LOG(debug) << "Found local multicast route, not add it";
            }
        }catch(std::exception& e){
            LOG(error) << "Exception"<< e.what();
        }
    }
    void route_add(int multicast_class, string nic)
    {
        string multicat_addr = to_string(multicast_class) + ".0.0.0";
        string netmask = "255.0.0.0";
        string cmd = "ip route add " + multicat_addr + "/8 dev " + nic;
        system(cmd);
    }
    void live_input_type_id(Mongo& db, live_setting& cfg, const string type)
    {
        try{
            cfg.type_id = 0;
            json input_types = json::parse(db.find_mony("live_inputs_types", "{}"));
            for(auto& t : input_types){
                if(t["name"] == type){
                    cfg.type_id =  t["_id"];
                    break;
                }
            }
        }catch(std::exception& e){
            LOG(error) << "Exception " << e.what();
        }
    }
    bool get_live_config(Mongo& db, live_setting& cfg, string type)
    {
        try{
            live_input_type_id(db, cfg, type);
            json net = json::parse(db.find_id("system_network",1));
            cfg.multicast_iface = "lo";
            cfg.main_iface = "";
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
            if(cfg.multicast_class > 239 || cfg.multicast_class < 224)
                cfg.multicast_class = 239;
            LOG(debug) 
                << "Live config:  "
                << " multicast_class:" << cfg.multicast_class 
                << " multicast_iface:" << cfg.multicast_iface 
                << " type_id:" << cfg.type_id;
            if(cfg.type_id == -1 || 
                    cfg.multicast_class > 239 ||
                    cfg.multicast_class < 224 ||
                    cfg.multicast_iface.size() < 1){
                LOG(error)  << "invalid config";
                return false;
            } 
            return true;
        }catch(std::exception& e){
            LOG(error) << "Exception " << e.what();
            cfg.multicast_class = 239;
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
            string path = string(MEDIA_ROOT);
            path += content_type["name"];
            path +=  "/";
            path += to_string(id);
            path += ".";
            path += content_info["format"].is_null() ? "" : content_info["format"];
            LOG(trace) << "Media Path:" << path;
            return path;
        }catch(std::exception& e){
            LOG(error) << "Exception " << e.what();
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
    void report_error(Mongo& db, const std::string msg, int level)
    {
        try{
            json j = json::object();
            j["_id"] = chrono::system_clock::now().time_since_epoch().count();
            j["time"] = long(time(nullptr));
            j["message"] = msg;
            j["message"] = "iptv_modules";
            j["level"] = level;
            db.insert("report_error", j.dump());
        }catch(std::exception const& e){
            LOG(error)  <<  e.what();
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
    bool chan_in_input(Mongo &db, int chan_id, int chan_type)
    {
        try{
            json filter;
            filter["active"] = true;
            filter["_id"] = chan_id;
            json in_network = json::parse(db.find_mony("live_inputs_network", filter.dump()));
            for(auto& chan : in_network){
                if(!chan["virtual"])
                    return true;
            }
            json in_dvb = json::parse(db.find_mony("live_inputs_dvb", filter.dump()));
            if(in_dvb.size() > 0)
                return true;

            json in_hdd = json::parse(db.find_mony("live_inputs_archive", filter.dump()));
            if(in_hdd.size() > 0)
                return true;

        }catch(std::exception const& e){
            LOG(error)  <<  e.what();
        }
        LOG(info) << "Not found channel id:" << chan_id << " in outputs"; 
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
            json out_dvb = json::parse(db.find_mony("live_output_dvb", filter.dump()));
            if(out_dvb.size() > 0)
                return true;

            json out_hdd = json::parse(db.find_mony("live_output_archive", filter.dump()));
            for(auto& chan : out_hdd){
                if(chan["timeShift"] > 0  || chan["programName"].size() > 0 )
                    return true;
            }
        }catch(std::exception const& e){
            LOG(error)  <<  e.what();
        }
        LOG(info) << "Not found channel id:" << chan_id << " in outputs"; 
        return false;
    }
}
