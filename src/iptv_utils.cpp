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
#include <boost/filesystem/operations.hpp>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>
#include <sys/stat.h>
#include <boost/filesystem.hpp>
#include "utils.hpp"
#include "iptv_utils_gst.hpp"
#define LOG_PERIOD 60
#define LICENSE_ID    "/opt/sms/lic.id"
#define LICENSE_JSON  "/opt/sms/lic.json"
using namespace std;

int check_license_db(Mongo& db);
void regenerate_id_and_json();
bool json_is_new();

int main(int argc, char *argv[])
{
    Mongo db;
    int check;
    int systemId = 0;
    int try_count = 0;
    bool system_stoped = false;

    if( geteuid() != 0 ){
        LOG(error) << "Must run by root";
        return -1;
    }
    Util::boost_log_init(db);
    systemId = check_license_db(db);
    if(argc > 1){
        LOG(info) << "Exit.";
        return 0;
    } 
    SysUsage usage;
    while(true){
        std::this_thread::sleep_for(chrono::seconds(LOG_PERIOD));

        if(!systemId || json_is_new()){
            systemId = check_license_db(db);
        }

        if(!license_capability_bool("GB_EPG", &check)){
            systemId = check_license_db(db);
            if(!systemId){
                try_count++;
                if(try_count > 5){
                    DB_ERROR(db, 1) << "Invalid license. Stop service!";
                    Util::system("/opt/sms/bin/sms e");
                    system_stoped = true;
                    try_count = 0;
                }
            }else{
                try_count = 0;
            }
        }
        if(system_stoped && systemId){
            DB_ERROR(db, 1) << "Valid license. Start service!";
            Util::system("/opt/sms/bin/sms s");
            system_stoped = false;
        }

        string usage_json = usage.getUsageJson(systemId);
        db.insert("report_system_usage", usage_json);
    }
}
bool json_is_new()
{
    try{
        struct stat st;

        if(!stat(LICENSE_JSON, &st)){
            return (time(nullptr) -  st.st_mtim.tv_sec) < LOG_PERIOD;
        }
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
    return true;
}
void regenerate_id_and_json()
{
    int check;
    LOG(info) << "Regenerte lic.id and lic.json!";
    try{
        boost::filesystem::remove(LICENSE_JSON);
        boost::filesystem::remove(LICENSE_ID);
    }catch(std::exception& e){
        LOG(error) << e.what();
    } 
    license_capability_bool("GB_EPG", &check);
}
int check_license_db(Mongo& db)
{
    string systemId = "0";
    json license = json::object();
    license["_id"] = 1;
    license["license"] = json::object();
    try{
        regenerate_id_and_json();
        string licStr = Util::get_file_content(LICENSE_JSON);
        if(!licStr.empty()){
            license["license"] = json::parse(licStr);
            if( !license["license"]["General"].is_null() ){
                LOG(info) << "Insert license to DB";
                systemId = license["license"]["General"]["MMK_ID"];
                db.insert_or_replace_id("system_license",1,license.dump());
            }
        }
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
    return stoi(systemId);
}
