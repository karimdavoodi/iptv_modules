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
bool gst_task(json, string in_multicast, int port, int maxPerChannel);
void remove_old_timeshift(Mongo& db, int maxPerChannel, const string channel_name);
void insert_content_info_db(Mongo &db,json& channel, uint64_t id);


void remove_old_timeshift(Mongo& db, int maxPerChannel, const string channel_name)
{
    json filter;
    filter["name"] = channel_name;
    filter["type"] = TIME_SHIFT_TYPE;
    long now = time(nullptr);

    LOG(debug) << "Remove old timeShift media before " << maxPerChannel;
    json channel_media = json::parse(db.find_mony("storage_contents_info", filter.dump()));

    LOG(trace) << "Filter:" << filter.dump(2) << " result count:" << channel_media.size();
    for(auto& media : channel_media){
        long media_date = media["date"];
        long late = now - media_date;
        if(late > maxPerChannel * RECORD_DURATION ){
            LOG(warning) << "Remove " << media["name"] << " of channel " << channel_name 
                << " for time " << media_date;
            uint64_t media_id = media["_id"];
            auto media_path = MEDIA_ROOT "TimeShift/" + 
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
void insert_content_info_db(Mongo &db,json& channel, uint64_t id)
{
    string name = channel["name"];
    json media = json::object();
    media["_id"] = id;
    media["format"] = MP4_FORMAT;
    media["type"] = TIME_SHIFT_TYPE;
    media["price"] = 0;
    media["date"] = time(nullptr);
    media["languages"] = json::array();
    media["permission"] = channel["permission"];
    media["platform"] = json::array();
    media["category"] = channel["category"];
    media["description"] = {
        {"en",{
                  { "name" ,name },
                  { "description" ,"" }
              }},
        {"fa",{
                  { "name" ,name },
                  { "description" ,"" }
              }},
        {"ar",{
                  { "name" ,name },
                  { "description" ,"" }
              }}
    };
    media["name"] = channel["name"];
    db.insert("storage_contents_info", media.dump());
    LOG(info) << "Record " << channel["name"] << ":" << name;

}
void start_channel(json channel, int maxPerChannel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);

    in_multicast = "229.2.68.223";
    try{
#if BY_FFMPEG
        uint64_t id = std::chrono::system_clock::now().time_since_epoch().count();
        auto file_path = boost::format("%s%s/%lld.mp4") 
            % MEDIA_ROOT % "TimeShift" % id ; 
#define FFMPEG_REC_OPTS  " -bsf:a aac_adtstoasc -movflags empty_moov -y -f mp4 "
        auto cmd = boost::format("%s -i udp://%s:%d -t 3600 -codec copy %s '%s'")
            % FFMPEG % in_multicast % INPUT_PORT % FFMPEG_REC_OPTS % file_path;
        Util::system(cmd.str());
#else
        gst_task(channel, in_multicast, INPUT_PORT, maxPerChannel); 
#endif

    }catch(std::exception& e){
        LOG(error) << e.what();
    }
    Util::wait(5000);
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
        json channels = json::parse(db.find_mony("live_output_archive", "{\"active\":true}"));
        for(auto& chan : channels ){
            IS_CHANNEL_VALID(chan);
            if(chan["timeShift"] > 0 && !chan["virtual"]){
                pool.emplace_back(start_channel, chan, maxPerChannel, live_config);
                //break;
                Util::wait(100);
            }
        }
        for(auto& t : pool)
            t.join();
    }else{
        LOG(warning) << "maxPerChannel == 0 ";
    }
    THE_END;
} 
