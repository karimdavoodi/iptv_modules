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
#include <thread>
#include <fstream>
#include <boost/filesystem.hpp>
#include "utils.hpp"
#include "iptv_utils_gst.hpp"
using namespace std;

const string system_usages();
int check_license_db(Mongo& db);


int main()
{
    Mongo db;
    int check;

    if( geteuid() != 0 ){
        LOG(error) << "Must run by root";
        return -1;
    }
    Util::boost_log_init(db);
    Util::system("rm -f /run/sms/*");
    license_capability_bool("GB_EPG", &check);
    // Report system usage
    int systemId = check_license_db(db);
    SysUsage usage;
    while(true){
        std::this_thread::sleep_for(chrono::seconds(60));
        string usage_json = usage.getUsageJson(systemId);
        db.insert("report_system_usage", usage_json);

        if(!license_capability_bool("GB_EPG", &check)){
            LOG(error) << "INVALID LICENSE!";
            Util::system("/opt/sms/bin/sms s");
        }
        if(!systemId)
            systemId = check_license_db(db);
    }
    return 0;
} 
int check_license_db(Mongo& db)
{
    string systemId = "0";
    json license = json::object();
    license["_id"] = 1;
    license["license"] = json::object();
    try{
        string licStr = Util::get_file_content("/run/sms/license.json");
        if(licStr.size()>0){
            license["license"] = json::parse(licStr);
            LOG(debug) << license.dump(2);
            systemId = license["license"]["General"]["MMK_ID"];
            db.insert_or_replace_id("system_license",1,license.dump());
        }
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
    return stoi(systemId);
}
