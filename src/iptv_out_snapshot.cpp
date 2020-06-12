#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#define SNAPSHOT_PERIOD 60
using namespace std;
bool gst_task(string in_multicast, int port, const string pic_path);
void start_channel(json channel, live_setting live_config)
{
    Mongo db;
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);
    int pic_id = channel["_id"].get<int>();
    auto pic_path = boost::format("%sSnapshot/%d.jpg")
        % MEDIA_ROOT % pic_id; 
    while(true){
        if(gst_task(in_multicast, INPUT_PORT, pic_path.str())){
            BOOST_LOG_TRIVIAL(info) << "Capture snapsot for " << channel["name"];
            string name = channel["name"].get<string>();
            json media = json::object();
            media["_id"] = pic_id;
            media["format"] = 10;  // jpg
            media["type"] = 10;    // Snapshot
            media["price"] = 0;
            media["date"] = time(NULL);
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
            media["name"] = name;
            db.insert_or_replace_id("storage_contents_info", pic_id, media.dump());
        }
        Util::wait(SNAPSHOT_PERIOD * 1000);
    }

}
int main()
{
    Mongo db;
    vector<thread> pool;
    live_setting live_config;
    CHECK_LICENSE;
    Util::init(db);
    if(!Util::get_live_config(db, live_config, "dvb")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }
    Util::check_path(string(MEDIA_ROOT) + "Snapshot");
    json silver_channels = json::parse(db.find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        IS_CHANNEL_VALID(chan);
        pool.emplace_back(start_channel, chan, live_config);
        Util::wait(1000);
    }
    for(auto& t : pool)
        t.join();
    THE_END;
} 
