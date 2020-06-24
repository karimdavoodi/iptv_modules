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
void check_license_db(Mongo& db)
{
    json license = json::object();
    license["_id"] = 1;
    license["license"] = json::object();
    try{
        string licStr = Util::get_file_content("/run/sms/license.json");
        if(licStr.size()>0){
            license["license"] = json::parse(licStr);
            LOG(debug) << license.dump(2);
            db.insert_or_replace_id("system_license",1,license.dump());
        }
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
}
int main()
{
    Mongo db;
    if( geteuid() != 0 ){
        LOG(error) << "Must run by root";
        return -1;
    }
    Util::boost_log_init(db);
    Util::system("rm -f /run/sms/*");
    CHECK_LICENSE;
    check_license_db(db);
    SysUsage usage;
    while(true){
        std::this_thread::sleep_for(chrono::seconds(60));
        string usage_json = usage.getUsageJson();
        //LOG(debug) << usage_json;
        db.insert("report_system_usage", usage_json);
    }
    return 0;
} 
