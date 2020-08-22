#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include "utils.hpp"
using namespace std;
void gst_task(string in_multicast, string out_multicast);
int main()
{
    Mongo db;

    CHECK_LICENSE;
    Util::init(db);
    json license = json::parse(db.find_id("system_license", 1));
    if(license["license"].is_null()){
        LOG(error) << "License in empty!";
        return -1;
    }
    if(license["license"]["LiveStreamer"]["LVS_Output_RTSP"] > 0){
        string nic_ip = "";
        json net = json::parse(db.find_id("system_network",1));
        int m_id = net["mainInterface"]; 
        for(auto& iface : net["interfaces"]){
            if(iface["_id"] == m_id){
                nic_ip = iface["ip"];
                break;
            }
        } 
        if(nic_ip.size() > 0){
            string cmd = "/opt/sms/bin/iptv2rtsp-proxy -l 10016 -s " + nic_ip;
            Util::system(cmd);
        }else{
            LOG(error) << "Main NIC IP not found";
        }
    }else{
        LOG(info) << "Dosen't have license for RTSP";
    }
    THE_END;
} 
