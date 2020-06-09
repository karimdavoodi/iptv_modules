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
void check_license_db()
{
    json license = json::object();
    license["_id"] = 1;
    license["license"] = json::object();
    try{
        string licStr = get_file_content("/run/sms/license.json");
        if(licStr.size()>0){
            license["license"] = json::parse(licStr);
            BOOST_LOG_TRIVIAL(debug) << license.dump(2);
            Mongo::insert_or_replace_id("system_license",1,license.dump());
        }
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << e.what();
    }
}
int main()
{
    if( geteuid() != 0 ){
        BOOST_LOG_TRIVIAL(error) << "Must run by root";
        return -1;
    }
    boost_log_init();
    system("rm -f /run/sms/*");
    CHECK_LICENSE;
    check_license_db();
    SysUsage usage;
    while(true){
        std::this_thread::sleep_for(chrono::seconds(6));
        string usage_json = usage.getUsageJson();
        //BOOST_LOG_TRIVIAL(trace) << usage_json;
        Mongo::insert("report_system_usage", usage_json);
    }
    return 0;
} 
