#include <boost/log/core/record_view.hpp>
#include <exception>    
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <boost/filesystem.hpp>
/*
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/core.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
*/
#include "utils.hpp"
using namespace std;
void boost_log_init()
{
    /*
    typedef boost::log::sinks::synchronous_sink< boost::log::sinks::syslog_backend > sink_t;
    boost::shared_ptr< boost::log::core > core = boost::log::core::get();
    boost::shared_ptr< boost::log::sinks::syslog_backend > backend(
            new boost::log::sinks::syslog_backend(
        boost::log::keywords::facility = boost::log::sinks::syslog::local0
    ));

    // Set the straightforward level translator for the "Severity" attribute of type int
    backend->set_severity_mapper(
            boost::log::sinks::syslog::direct_severity_mapping< int >("Severity"));
    core->add_sink(boost::make_shared<sink_t>(backend));
    */
    // Set Debug level
    
    json system_location = json::parse(Mongo::find_id("system_location",1));
    int debug_level = system_location["debug"];
    debug_level = abs(5-debug_level);
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= debug_level);

}
void init()
{
    if( geteuid() != 0 ){
        BOOST_LOG_TRIVIAL(error) << "Must run by root";
        exit(-1);
    }
    boost_log_init();
    Gst::init();
    // Add internal multicast net to localhost 
    route_add(INPUT_MULTICAST, "lo");
}
void route_add(int multicast_class, string nic)
{
    string multicat_addr = to_string(multicast_class) + ".0.0.0";
    string netmask = "255.0.0.0";
    string cmd = "route add -net " + multicat_addr + " netmask 255.0.0.0 dev " + nic; 
    BOOST_LOG_TRIVIAL(info) << cmd;
    std::system(cmd.c_str());
}
int live_input_type_id(const string type)
{
    json input_types = json::parse(Mongo::find("live_inputs_types", "{}"));
    for(auto& t : input_types){
        if(t["name"] == type){
            return t["_id"];
        }
    }
    return -1;
}
bool get_live_config(live_setting& cfg, string type)
{
    json net = json::parse(Mongo::find_id("system_network",1));
    int m_id = net["multicastInterface"]; 
    for(auto& iface : net["interfaces"]){
        if(iface["_id"] == m_id){
            cfg.multicast_iface = iface["name"];
            break;
        }
    } 
    string addr = net["multicastBase"];
    string num1 = addr.substr(0, addr.find('.'));
    cfg.multicast_class = std::stoi(num1);
    cfg.type_id = live_input_type_id(type);
    BOOST_LOG_TRIVIAL(trace) 
        << "Live config:  "
        << " multicast_class:" << cfg.multicast_class 
        << " multicast_iface:" << cfg.multicast_iface 
        << " type_id:" << cfg.type_id;
    if(cfg.type_id == -1 || 
            cfg.multicast_class > 254 ||
            cfg.multicast_class < 224 ||
            cfg.multicast_iface.size() < 1
            ) return false;
    return true;
}
string get_multicast(live_setting& config, int channel_id, bool out_multicast)  
{
    uint32_t address = 0;
    address  = out_multicast ? config.multicast_class : INPUT_MULTICAST ;
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
}
void check_path(const std::string path)
{
    if(!boost::filesystem::exists(path)){
        BOOST_LOG_TRIVIAL(info) << "Create " << path;
        boost::filesystem::create_directories(path);
    }
}
