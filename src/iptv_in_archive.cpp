#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include <boost/filesystem/operations.hpp>
#include "utils.hpp"
#define TEST_BY_FFMPEG 0

using namespace std;

void gst_task(string media_path, string multicast_addr, int port);
bool time_to_play(Mongo& db, json& media);
int current_time();
void update_epg(Mongo& db, int64_t silver_chan_id, int64_t content_id);
void start_channel(json channel, live_setting live_config);
/*
 *   The main()
 *      - check license
 *      - read channels from mongoDB 
 *      - start thread for each active channel
 *      - wait to join
 * */
int main()
{
    vector<thread> pool;
    live_setting live_config;
    Mongo db;

    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "archive")){
        LOG(error) << "Error in live config! Exit.";
        return -1;
    }
    json channels = json::parse(db.find_mony("live_inputs_archive", 
                "{\"active\":true}"));
    for(auto& chan : channels ){
        IS_CHANNEL_VALID(chan);
        if(Util::chan_in_output(db, chan["_id"], live_config.type_id)){
            pool.emplace_back(start_channel, chan, live_config);
            break;
        }
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
/*
 *  The channel thread function
 *  Play Archive channel forever. play media of channel if match the time
 *  @param channel : config of channel
 *  @param live_config : general live streamer config
 *
 * */
void start_channel(json channel, live_setting live_config)
{
    Mongo db;
    try{
        int64_t channel_id = channel["_id"];
        LOG(info) << "Start Channel: " << channel["name"];
        LOG(info) <<  channel.dump(2);
        auto multicast = Util::get_multicast(live_config, channel["_id"]);
        while(true){ 
            for(auto& media : channel["contents"]){
                if(time_to_play(db, media)){
                    LOG(debug) << "Play media id: " << media["content"];
                    auto media_path = Util::get_content_path(db, media["content"]);
                    if(media_path.size() == 0){
                        LOG(error) << "Invalid media path";
                        Util::wait(50000);
                        continue;
                    } 
                    if(!boost::filesystem::exists(media_path)){
                        LOG(error) << "Media not exists:" << media_path;
                        Util::wait(50000);
                        continue;
                    }
                    update_epg(db, channel_id, media["content"]);

#if TEST_BY_FFMPEG
                    auto cmd = boost::format("%s -re -i '%s' -codec copy -f mpegts "
                            "'udp://%s:%d?pkt_size=1316'")
                        % FFMPEG 
                        % media_path 
                        % multicast
                        % INPUT_PORT;
                    Util::system(cmd.str());
#else
                    gst_task(media_path, multicast, INPUT_PORT);
#endif
                }else{
                    LOG(debug) << "Not play media id: " << media["content"]
                        << " due to time.";
                }
            }
            Util::wait(5000);
        }
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }

}
/*
 *   Check time of 'media'
 *   @return 'true' if time of 'media' match by current time
 *   
 * */
bool time_to_play(Mongo& db, json& media)
{
    int start = media["startDate"];
    int end = media["endDate"];

    auto now = time(nullptr);
    if(now > end || now < start){
        LOG(debug) << "Content " << media["content"] << " not play, due to time period.";
        return false;
    } 

    if(Util::check_weektime(db, media["weektime"] )) 
        return true;
    LOG(debug) << "Content " << media["content"] << " not play, due to weektime.";
    return false;
}
/*
 *  @return second elapse from 00:00:00
 * */
int current_time()
{
    auto now = time(nullptr);
    auto tm = localtime(&now);
    int  start = 0;
    start = 3600 * tm->tm_hour;
    start += 60 * tm->tm_min;
    start += tm->tm_sec;
    return start;
}
/*
 *   Update EPG of archive channel in DB
 * */
void update_epg(Mongo& db, int64_t silver_chan_id, int64_t content_id)
{
    json content_info = json::parse(db.find_id("storage_contents_info",content_id)); 
    json j;
    j["start"] = current_time();
    j["name"] = content_info["name"];
    j["duration"] = 0; // FIXME: real duration!
    j["text"] = "";
    if(!content_info["description"].is_null() &&
            !content_info["description"]["en"].is_null() &&
            !content_info["description"]["en"]["description"].is_null() )
        j["text"] = content_info["description"]["en"]["description"];
    json epg = json::object();
    epg["_id"] = silver_chan_id;
    epg["total"] = 1;
    epg["content"] = json::array();
    epg["content"].push_back(j);
    db.insert_or_replace_id("live_output_silver_epg", silver_chan_id, epg.dump());
    LOG(info) << "Update EPG of channel_id:" << silver_chan_id;
}
