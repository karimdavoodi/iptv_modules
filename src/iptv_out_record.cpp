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

bool gst_convert_udp_to_mp4(json, string in_multicast, int port, int maxPerChannel);
void start_channel(json channel, int maxPerChannel, live_setting live_config);

/*
 *   The main()
 *      - check license
 *      - read channels from mongoDB 
 *      - start thread for each active channel
 *      - wait to join
 * */
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
        json channels = json::parse(db.find_mony("live_output_archive", 
                    "{\"active\":true}"));
        for(auto& chan : channels ){
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
    Util::wait_forever();
} 
/*
 *  The channel thread function
 *  
 *  @param channel : config of channel
 *  @param maxPerChannel: time in houre to expire media files
 *  @param live_config : general live streamer config
 *
 * */
void start_channel(json channel, int maxPerChannel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = Util::get_multicast(live_config, channel["input"]);

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
        channel["name"] = Util::get_channel_name(channel["input"], channel["inputType"]);
        gst_convert_udp_to_mp4(channel, in_multicast, INPUT_PORT, maxPerChannel); 
#endif

    }catch(std::exception& e){
        LOG(error) << e.what();
    }
    Util::wait(5000);
}
