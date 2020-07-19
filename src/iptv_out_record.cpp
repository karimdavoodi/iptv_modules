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
#define TIME_SHIFT_TYPE 9
#define MP4_FORMAT 2
using namespace std;
bool gst_task(string in_multicast, int port,  string file_path, int second);

void remove_old_timeshift(Mongo& db, int maxPerChannel, const string channel_name)
{
    json filter;
    filter["name"] = channel_name;
    filter["type"] = TIME_SHIFT_TYPE;
    long now = time(nullptr);

    json channel_media = json::parse(db.find_mony("storage_contents_info", filter.dump()));

    LOG(trace) << "Filter:" << filter.dump(2) << " result count:" << channel_media.size();
    for(auto& media : channel_media){
        long media_date = media["date"];
        long late = now - media_date;
        if(late > maxPerChannel*3600 ){
            LOG(warning) << "Remove " << media["name"] << " of channel " << channel_name 
                << " for time " << media_date;
            uint64_t media_id = media["_id"];
            auto media_path = "/opt/sms/www/iptv/media/TimeShift/" + 
                                                to_string(media_id) + ".mp4";
            db.remove_id("storage_contents_info", media_id);
            if(boost::filesystem::exists(media_path)){
                boost::filesystem::remove(media_path);
                LOG(info) << "TimeShift file removed:" << media_path;
            }else{
                LOG(error) << "TimeShift file not exists:" << media_path;
            }
        }
    }
}
void start_channel(json channel, int maxPerChannel, live_setting live_config)
{
    Mongo db;
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    
    //in_multicast = "229.2.68.223";
    while(true){
        try{
            string ch_name = channel["name"];
            remove_old_timeshift(db, maxPerChannel, ch_name);
            auto id = std::chrono::system_clock::now().time_since_epoch().count();
            auto now = time(nullptr);
            auto tm = localtime(&now);
            auto name = boost::format("%d_%d_%d-%d_%d") % 
                (1900+tm->tm_year) % 
                tm->tm_mon % 
                tm->tm_mday % 
                tm->tm_hour % 
                tm->tm_min;
            auto file_path = boost::format("%s%s/%lld.mp4") 
                % MEDIA_ROOT % "TimeShift" % id ; 
            int duration = (60 - tm->tm_min)*60;
            bool recorded = true;
#if BY_FFMPEG
            #define FFMPEG_REC_OPTS  " -bsf:a aac_adtstoasc -movflags empty_moov -y -f mp4 "
            auto cmd = boost::format("%s -i udp://%s:%d -t %d -codec copy %s '%s'")
                % FFMPEG % in_multicast % INPUT_PORT % duration % FFMPEG_REC_OPTS % file_path;
            Util::system(cmd.str());
#else
            recorded = gst_task(in_multicast, INPUT_PORT ,file_path.str(), duration); 
#endif
            if(recorded){
                json media = json::object();
                media["_id"] = id;
                media["format"] = MP4_FORMAT;
                media["type"] = TIME_SHIFT_TYPE;
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
                media["name"] = ch_name;
                db.insert("storage_contents_info", media.dump());
                LOG(info) << "Record " << ch_name << ":" << name.str();
            }else{
                LOG(error) << "Not record " << ch_name << ":" << name.str();
            }
        }catch(std::exception& e){
            LOG(error) << e.what();
        }
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
    json storage_setting = json::parse(db.find_id("storage_setting", 1));
    int maxPerChannel = 0;
    if(!storage_setting["timeShift"].is_null() )
        maxPerChannel = storage_setting["timeShift"]["maxPerChannel"];

    if(maxPerChannel > 0){
        json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
        for(auto& chan : silver_channels ){
            IS_CHANNEL_VALID(chan);
            if(chan["recordTime"] > 0){
                if(chan["inputType"] != live_config.virtual_dvb_id &&
                        //chan["inputType"] == live_config.type_id  && 
                        chan["inputType"] != live_config.virtual_net_id  ){
                    pool.emplace_back(start_channel, chan, maxPerChannel, live_config);
                    //break;
                    Util::wait(100);
                }
            }
        }
        for(auto& t : pool)
            t.join();
    }else{
        LOG(warning) << "maxPerChannel == 0 ";
    }
    THE_END;
} 
