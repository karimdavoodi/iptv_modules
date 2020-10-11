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
void SysUsage::calcCurrentUsage()
{
    calcCurrentPartitions();
    calcCurrentInterfaces();
    calcCurrentCpu();
    calcCurrentMem();
    calcCurrentLoad();
    calcCurrentContents();
}
const string SysUsage::getUsageJson(int systemId)
{
    Data delta;
    priviuse = current;
    try{
        calcCurrentUsage();
        // CPU Usage
        float totald = current.cpuTotal - priviuse.cpuTotal;
        float idled = current.cpuIdle - priviuse.cpuIdle;
        delta.cpuUsage = (totald - idled) / totald;
        // Disk Usage
        for(const auto& [name, transfering] : current.partitions){
            transfer d;
            d.read = transfering.read - priviuse.partitions[name].read;
            d.write = transfering.write - priviuse.partitions[name].write;
            delta.partitions[name] = d;
        }
        // Network Usage
        for(const auto& [name, transfering] : current.interfaces){
            transfer d;
            d.read = transfering.read - priviuse.interfaces[name].read;
            d.write = transfering.write - priviuse.interfaces[name].write;
            delta.interfaces[name] = d;
        }
        // Total Disk Usage
        float diskUsage = 0;
        string percent = Util::shell_out("df /opt/sms/www/iptv/media/Video "
                                      "| tail -1 | awk '{print $5}' ");
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
        usage["systemId"] = systemId;
        usage["sysLoad"] = current.sysLoad;
        usage["cpuUsage"] = delta.cpuUsage;
        usage["memUsage"] = (current.memTotal - current.memAvailable) / current.memTotal;
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
        json contents = json::object();
        for(const auto& content : current.contents){
            contents[content.first] = content.second; 
        }
        usage["contents"] = contents;

        return usage.dump(2);
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
    return "{}";
}
void SysUsage::calcCurrentPartitions()
{
    string line;
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
    for(const auto& dir : boost::filesystem::directory_iterator("/sys/class/net")){
        string name = dir.path().filename().c_str();
        string rx_file = "/sys/class/net/"+name+"/statistics/rx_bytes";
        string tx_file = "/sys/class/net/"+name+"/statistics/tx_bytes";
        auto read = Util::get_file_content(rx_file);
        if(read.size()){
            current.interfaces[name].read = stof(read);
        }
        auto write = Util::get_file_content(tx_file);
        if(write.size()){
            current.interfaces[name].write = stof(write);
        }
    }
}
void SysUsage::calcCurrentCpu()
{
    string line;
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
    ifstream load("/proc/loadavg");
    if(!load.is_open()) return;
    load >> current.sysLoad;
}
void SysUsage::calcCurrentContents()
{
    long all_size = 1024 * stol(Util::shell_out(
                "df /opt/sms/www/iptv/media/Video | tail -1 | awk '{print $2}' ")) ;
    current.contents["All"] = all_size;
    for(const auto& content : contents_dir){
        auto path = "/opt/sms/www/iptv/media/" + content;
        if(boost::filesystem::exists(path)){
            current.contents[content] = stol(Util::shell_out(
                    "du -sb " + path + "| tail -1 | awk '{print $1}' ")) ;
        }else{
            current.contents[content] = 0;
        }
    }
}
