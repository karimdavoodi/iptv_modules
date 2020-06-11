#include <fstream>
#include <string>
#include <chrono>
#include <array>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include "../third_party/json.hpp"
#include "iptv_utils_gst.hpp"
#include "utils.hpp"
using namespace std;
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
void SysUsage::calcCurrentUsage()
{
    BOOST_LOG_TRIVIAL(debug) << __func__;
    calcCurrentPartitions();
    calcCurrentInterfaces();
    calcCurrentCpu();
    calcCurrentMem();
    calcCurrentLoad();
}
const string SysUsage::getUsageJson()
{
    Data delta;
    priviuse = current;
    BOOST_LOG_TRIVIAL(debug) << __func__;
    try{
        calcCurrentUsage();
        // CPU Usage
        float totald = current.cpuTotal - priviuse.cpuTotal;
        float idled = current.cpuIdle - priviuse.cpuIdle;
        delta.cpuUsage = (totald - idled) / totald;
        // Disk Usage
        for(const auto& curr : current.partitions){
            transfer d;
            string name = curr.first;
            d.read =curr.second.read - priviuse.partitions[name].read;
            d.write =curr.second.write - priviuse.partitions[name].write;
            delta.partitions[name] = d;
        }
        // Network Usage
        for(const auto& curr : current.interfaces){
            transfer d;
            string name = curr.first;
            d.read =curr.second.read - priviuse.interfaces[name].read;
            d.write =curr.second.write - priviuse.interfaces[name].write;
            delta.interfaces[name] = d;
        }
        // Total Disk Usage
        float diskUsage = 0;
        string line = exec("df -l --total | tail -1");
        boost::tokenizer<> tok(line);
        auto it = tok.begin(); ++it; ++it; ++it; ++it;
        string percent = *it;
        if(percent.find('%') != string::npos)
            diskUsage = stof(percent.substr(0,percent.find('%')));
        else
            diskUsage = stof(percent);
        // Build JSON
        using nlohmann::json; 
        json usage = json::object();
        auto now = chrono::system_clock::now(); 
        now.time_since_epoch().count();
        usage["_id"] = now.time_since_epoch().count();
        usage["time"] = now.time_since_epoch().count()/1000000000;
        usage["sysLoad"] = current.sysLoad;
        usage["cpuUsage"] = delta.cpuUsage;
        usage["memUsage"] = (current.cpuTotal - current.memAvailable) / current.memTotal;
        usage["diskUsage"] = diskUsage / 100;
        usage["networkInterfaces"] = json::array();
        for(const auto& interface : delta.interfaces){
            json net = json::object();
            net["name"] = interface.first;
            net["read"] = int(interface.second.read);
            net["write"] = int(interface.second.write);
            usage["networkInterfaces"].push_back(net);
        }
        usage["diskPartitions"] = json::array();
        for(const auto& partition : delta.partitions){
            json part = json::object();
            part["name"] = partition.first;
            part["read"] = int(partition.second.read);
            part["write"] = int(partition.second.write);
            usage["diskPartitions"].push_back(part);
        }
        return usage.dump(2);
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << e.what();
    }
    return "{}";
}
void SysUsage::calcCurrentPartitions()
{
    string line;
    BOOST_LOG_TRIVIAL(debug) << __func__;
    ifstream disk("/proc/diskstats");
    while(disk.good()){
        getline(disk, line);
        if(line.find("sd") == string::npos) continue;
        boost::tokenizer<> tok(line);
        auto it = tok.begin();
        ++it; 
        string name = *(++it);
        current.partitions[name].read = stof(*(++it));
        ++it; ++it; ++it; 
        current.partitions[name].write = stof(*(++it));
    }
}
void SysUsage::calcCurrentInterfaces()
{
    BOOST_LOG_TRIVIAL(debug) << __func__;
    for(const auto& dir : boost::filesystem::directory_iterator("/sys/class/net")){
        string name = dir.path().filename().c_str();
        string rx_file = "/sys/class/net/"+name+"/statistics/rx_bytes";
        string tx_file = "/sys/class/net/"+name+"/statistics/tx_bytes";
        auto read = Util::get_file_content(rx_file);
        if(read.size()){
            current.interfaces[name].read = stof(read);
        }
        auto write = Util::get_file_content(rx_file);
        if(write.size()){
            current.interfaces[name].write = stof(write);
        }
    }
}
void SysUsage::calcCurrentCpu()
{
    string line;
    BOOST_LOG_TRIVIAL(debug) << __func__;
    ifstream stat("/proc/stat");
    if(!stat.is_open()) return;
    getline(stat, line);
    boost::tokenizer<> tok(line);
    auto it = tok.begin();
    float user = stof(*(++it));
    float nice = stof(*(++it));
    float system = stof(*(++it));
    float idle = stof(*(++it));
    float iowait = stof(*(++it));
    float irq = stof(*(++it));
    float softirq = stof(*(++it));
    float steal = stof(*(++it));
    float guest = stof(*(++it));
    float guest_nice = stof(*it);
    idle = idle + iowait;
    float nonIdle = user + nice + system + irq + softirq + steal;
    current.cpuIdle = idle;
    current.cpuTotal = idle + nonIdle;
}
void SysUsage::calcCurrentMem()
{
    float total = 0;
    float available = 0;
    string line;
    BOOST_LOG_TRIVIAL(debug) << __func__;
    ifstream mem("/proc/meminfo");
    while(mem.good()){
        getline(mem, line);
        if(line.find("MemTotal") != string::npos){
            boost::tokenizer<> tok(line);
            auto it = tok.begin();
            total = stof(*(++it));
        }
        if(line.find("MemAvailable") != string::npos){
            boost::tokenizer<> tok(line);
            auto it = tok.begin();
            available = stof(*(++it));
            break;
        }
    }
    current.memTotal = total;
    current.memAvailable = available;
}
void SysUsage::calcCurrentLoad()
{
    string line;
    BOOST_LOG_TRIVIAL(debug) << __func__;
    ifstream load("/proc/loadavg");
    if(!load.is_open()) return;
    load >> current.sysLoad;
}
