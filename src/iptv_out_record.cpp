#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>
#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include "utils.hpp"
#define BY_FFMPEG 0
using namespace std;
void gst_task(string in_multicast, int port,  string file_path, int second);
void start_channel(json channel, live_setting live_config)
{
    Mongo db;
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    while(true){
        string ch_name = channel["name"];
        auto id = std::chrono::system_clock::now().time_since_epoch().count();
        auto now = time(NULL);
        auto tm = localtime(&now);
        auto name = boost::format("%s-%d_%d_%d-%d_%d") % 
            ch_name %
            (1900+tm->tm_year) % 
            tm->tm_mon % 
            tm->tm_mday % 
            tm->tm_hour % 
            tm->tm_min;
        auto file_path = boost::format("%s%s/%lld.mp4") 
            % MEDIA_ROOT % "TimeShift" % id ; 
        int duration = (60 - tm->tm_min)*60;
        
#if BY_FFMPEG
        #define FFMPEG_REC_OPTS  " -bsf:a aac_adtstoasc -movflags empty_moov -y -f mp4 "
        auto cmd = boost::format("%s -i udp://%s:%d -t %d -codec copy %s '%s'")
            % FFMPEG % in_multicast % INPUT_PORT % duration % FFMPEG_REC_OPTS % file_path;
        Util::system(cmd.str());
#else
        gst_task(in_multicast, INPUT_PORT ,file_path.str(), duration); 
#endif
        json media = json::object();
        media["_id"] = std::chrono::system_clock::now().time_since_epoch().count();
        media["format"] = 2; // mp4
        media["type"] = 9; // TimeShift
        media["price"] = 0;
        media["date"] = now;
        media["languages"] = json::array();
        media["permission"] = channel["permission"];
        media["platform"] = json::array();
        media["category"] = channel["category"];
        media["description"] = {
            {"en",{
                   { "name" ,name.str() },
                   { "description" ,"" }
            }},
            {"fa",{
                   { "name" ,name.str() },
                   { "description" ,"" }
            }},
            {"ar",{
                   { "name" ,name.str() },
                   { "description" ,"" }
            }}
        };
        media["name"] = name.str();
        db.insert("storage_contents_info", media.dump());
        LOG(info) << "Record " << name.str();
        Util::wait(1000);
    }
}
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    string time_shift_dir = string(MEDIA_ROOT) + "TimeShift";
    if(!boost::filesystem::exists(time_shift_dir)){
        LOG(info) << "Create " << time_shift_dir;
        boost::filesystem::create_directory(time_shift_dir);
    }
    if(!Util::get_live_config(db, live_config, "archive")){
        LOG(info) << "Error in live config! Exit.";
        return -1;
    }
    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        if(chan["active"] == true && chan["recordTime"] > 0){
            if(chan["inputType"] != live_config.virtual_dvb_id &&
                    chan["inputType"] == live_config.type_id  && 
                    chan["inputType"] != live_config.virtual_net_id  ){
                
                pool.emplace_back(start_channel, chan, live_config);
                break;
                Util::wait(100);
            }
                
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
