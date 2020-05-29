#include <boost/log/core/record_view.hpp>
#include <exception>    
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
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

void boost_log_init()
{
    namespace logging = boost::log;
    namespace keywords = logging::keywords;
    namespace attrs = logging::attributes;
    logging::add_common_attributes();
    logging::core::get()->add_global_attribute(
            "Process", attrs::current_process_name());

    logging::add_file_log
        (
         keywords::file_name = "/opt/sms/tmp/log.log",
         keywords::format = "%Process% %ThreadID%: %Message%",
         keywords::auto_flush = true,
         keywords::open_mode = std::ios_base::app
         //%TimeStamp% %Process% %ThreadID% %Severity% %LineID% %Message%"     
        );

    // Set Debug level
    json system_location = json::parse(Mongo::find_id("system_location",1));
    int debug_level = system_location["debug"];
    debug_level = abs(5-debug_level);
    logging::core::get()->set_filter(
            logging::trivial::severity >= debug_level);

}
void init()
{
    try{
        if( geteuid() != 0 ){
            BOOST_LOG_TRIVIAL(error) << "Must run by root";
            exit(-1);
        }
        boost_log_init();
        Gst::init();
        // Add internal multicast net to localhost 
        route_add(INPUT_MULTICAST, "lo");
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
    }
}
void db_log(const std::string msg, int type)
{
    long timestamp = time(NULL);
    long rand = std::rand() % 100000;
    if(type == ERROR){
        json j = {
            {"_id", timestamp + rand },
            {"time", timestamp },
            {"error", msg},
            {"priority", 0}
        };
        Mongo::insert("status_errors", j.dump());
    }else{
        json j = {
            {"_id", timestamp + rand },
            {"time", timestamp },
            {"activity", msg}
        };
        Mongo::insert("report_system", j.dump());
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
void live_input_type_id(live_setting& cfg, const string type)
{
    try{
        json input_types = json::parse(Mongo::find_mony("live_inputs_types", "{}"));
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
bool get_live_config(live_setting& cfg, string type)
{
    try{
        live_input_type_id(cfg, type);
        json net = json::parse(Mongo::find_id("system_network",1));
        cfg.multicast_class = net["multicastBase"];
        int m_id = net["multicastInterface"]; 
        for(auto& iface : net["interfaces"]){
            if(iface["_id"] == m_id){
                cfg.multicast_iface = iface["name"];
                break;
            }
        } 
        BOOST_LOG_TRIVIAL(trace) 
            << "Live config:  "
            << " multicast_class:" << cfg.multicast_class 
            << " multicast_iface:" << cfg.multicast_iface 
            << " type_id:" << cfg.type_id;
        if(cfg.type_id == -1 || 
                cfg.multicast_class > 254 ||
                cfg.multicast_class < 224 ||
                cfg.multicast_iface.size() < 1){

            BOOST_LOG_TRIVIAL(error)  << "Error in " << __func__;
            return false;
        } 
        return true;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
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
    BOOST_LOG_TRIVIAL(trace) 
        << " multicast_class:" << config.multicast_class 
        << " type_id:" << config.type_id 
        << " channel_id: " << channel_id
        << " --> " << addr_str;
    return addr_str;
}
string get_content_path(int id)
{
    try{
        json content_info = json::parse(Mongo::find_id("storage_contents_info",id));
        if(content_info["type"].is_null()){
            BOOST_LOG_TRIVIAL(info) << "Invalid content info by id " << id;
            return "";
        }
        json content_type = json::parse(Mongo::find_id("storage_contents_types",
                    content_info["type"]));
        json content_format = json::parse(Mongo::find_id("storage_contents_formats",
                    content_info["format"]));
        BOOST_LOG_TRIVIAL(trace) << content_info  << '\n'
            << content_type << '\n'
            << content_format << '\n';
        string path = string(MEDIA_ROOT);
        path += content_type["name"];
        path +=  "/";
        path += to_string(id);
        path += ".";
        path += content_format["name"];
        BOOST_LOG_TRIVIAL(trace) << "Media Path:" << path;
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
