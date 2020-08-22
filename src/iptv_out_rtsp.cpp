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
