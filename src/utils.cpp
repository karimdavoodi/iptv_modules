#include <boost/log/core/record_view.hpp>
#include <exception>    
#include <iostream>
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
using namespace std;
namespace Util {
    void wait(int millisecond)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(millisecond));
    }
    void exec_shell_loop(const std::string cmd)
    {
        while(true){
            BOOST_LOG_TRIVIAL(info) << cmd;
            std::system(cmd.c_str());
            std::this_thread::sleep_for(std::chrono::seconds(5));
            BOOST_LOG_TRIVIAL(error) << "Rerun: "<<  cmd;
        }
    }
    void boost_log_init(Mongo& db)
    {
        try{
            namespace logging = boost::log;
            namespace keywords = logging::keywords;
            namespace attrs = logging::attributes;
            logging::add_common_attributes();
            logging::core::get()->add_global_attribute(
                    "Process", attrs::current_process_name());
            // Set Debug level
            json system_location = json::parse(db.find_id("system_hidden",1));
            int debug_level = (!system_location["debug"].is_null())? 
                system_location["debug"].get<int>():5;
            string out_file = "/dev/stdout";
            if(debug_level < 5) out_file = "/opt/sms/tmp/log.log"; 
            debug_level = abs(5-debug_level);
            BOOST_LOG_TRIVIAL(debug) << "Log file " << out_file;
            logging::add_file_log
                (
                 keywords::file_name = out_file,
                 keywords::format = "%Process% %ThreadID%: %Message%",
                 keywords::auto_flush = true,
                 keywords::open_mode = std::ios_base::app
                 //%TimeStamp% %Process% %ThreadID% %Severity% %LineID% %Message%"     
                );
            logging::core::get()->set_filter(
                    logging::trivial::severity >= debug_level);
        }catch(std::exception& e){
            BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        }
    }
    void init(Mongo& db)
    {
        try{
            boost_log_init(db);
            Gst::init();
            // Add internal multicast net to localhost 
            if( geteuid() == 0 ){
                route_add(INPUT_MULTICAST, "lo");
            }
        }catch(std::exception& e){
            BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        }
    }
    void route_add(int multicast_class, string nic)
    {
        string multicat_addr = to_string(multicast_class) + ".0.0.0";
        string netmask = "255.0.0.0";
        string cmd = "route add -net " + multicat_addr + " netmask 255.0.0.0 dev " + nic; 
        BOOST_LOG_TRIVIAL(info) << cmd;
        std::system(cmd.c_str());
    }
    void live_input_type_id(Mongo& db, live_setting& cfg, const string type)
    {
        try{
            json input_types = json::parse(db.find_mony("live_inputs_types", "{}"));
            for(auto& t : input_types){
                if(t["name"] == type)
                    cfg.type_id =  t["_id"];
                if(t["name"] == "virtual_dvb")
                    cfg.virtual_dvb_id =  t["_id"];
                if(t["name"] == "virtual_net")
                    cfg.virtual_net_id =  t["_id"];
            }
        }catch(std::exception& e){
            BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        }
    }
    bool get_live_config(Mongo& db, live_setting& cfg, string type)
    {
        try{
            live_input_type_id(db, cfg, type);
            json net = json::parse(db.find_id("system_network",1));
            cfg.multicast_iface = "lo";
            cfg.multicast_class = net["multicastBase"];
            int m_id = net["multicastInterface"]; 
            for(auto& iface : net["interfaces"]){
                if(iface["_id"] == m_id){
                    cfg.multicast_iface = iface["name"];
                    break;
                }
            } 
            BOOST_LOG_TRIVIAL(debug) 
                << "Live config:  "
                << " multicast_class:" << cfg.multicast_class 
                << " multicast_iface:" << cfg.multicast_iface 
                << " type_id:" << cfg.type_id;
            if(cfg.type_id == -1 || 
                    cfg.multicast_class > 239 ||
                    cfg.multicast_class < 224 ||
                    cfg.multicast_iface.size() < 1){
                BOOST_LOG_TRIVIAL(error)  << "Error in " << __func__;
                return false;
            } 
            return true;
        }catch(std::exception& e){
            BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
            cfg.multicast_class = 239;
            return false;
        }
    }
    string get_multicast(live_setting& config, int channel_id, bool out_multicast)  
    {
        uint32_t address = 0;
        address  = out_multicast ? config.multicast_class : INPUT_MULTICAST ;
        if(address < 224 || address > 239) address = 239;
        address += config.type_id << 8;
        address += (channel_id & 0x0000ff00) << 16;
        address += (channel_id & 0x000000ff) << 24;
        struct in_addr addr;
        addr.s_addr = address;
        string addr_str =  inet_ntoa(addr);
        BOOST_LOG_TRIVIAL(debug) 
            << " multicast_class:" << config.multicast_class 
            << " type_id:" << config.type_id 
            << " channel_id: " << channel_id
            << " --> " << addr_str;
        return addr_str;
    }
    string get_content_path(Mongo& db, int id)
    {
        try{
            json content_info = json::parse(db.find_id("storage_contents_info",id));
            if(content_info["type"].is_null()){
                BOOST_LOG_TRIVIAL(warning) << "Invalid content info by id " << id;
                return "";
            }
            json content_type = json::parse(db.find_id("storage_contents_types",
                        content_info["type"]));
            json content_format = json::parse(db.find_id("storage_contents_formats",
                        content_info["format"]));
            BOOST_LOG_TRIVIAL(debug) << content_info  << '\n'
                << content_type << '\n'
                << content_format << '\n';
            string path = string(MEDIA_ROOT);
            path += content_type["name"];
            path +=  "/";
            path += to_string(id);
            path += ".";
            path += content_format["name"];
            BOOST_LOG_TRIVIAL(debug) << "Media Path:" << path;
            return path;
        }catch(std::exception& e){
            BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
            return "";
        }
    }
    void check_path(const std::string path)
    {
        if(!boost::filesystem::exists(path)){
            BOOST_LOG_TRIVIAL(info) << "Create " << path;
            boost::filesystem::create_directories(path);
        }
    }
    void report_error(Mongo& db, const std::string msg, int level)
    {
        try{
            json j = json::object();
            j["_id"] = chrono::system_clock::now().time_since_epoch().count();
            j["time"] = long(time(NULL));
            j["message"] = msg;
            j["message"] = "iptv_modules";
            j["level"] = level;
            db.insert("report_error", j.dump());
        }catch(std::exception const& e){
            BOOST_LOG_TRIVIAL(error)  <<  e.what();
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
        BOOST_LOG_TRIVIAL(warning)  <<  "file not exists: " << name;
        return "";
    }
}
