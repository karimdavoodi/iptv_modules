#include <boost/filesystem/operations.hpp>
#include <boost/format/format_fwd.hpp>
#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <boost/format.hpp>
#include "utils.hpp"
#define BY_FFMPEG 0
using namespace std;
void gst_task(string in_multicast, string file_path, int second);

void start_channel(json channel, live_setting live_config)
{
    live_config.type_id = channel["inputType"];
    auto in_multicast = get_multicast(live_config, channel["inputId"]);

    while(true){
        auto now = time(NULL);
        auto tm = localtime(&now);
        auto date_fmt = boost::format("%d_%d_%d-%d_%d") 
            % (1900+tm->tm_year) % tm->tm_mon % tm->tm_mday % tm->tm_hour % tm->tm_min;
        auto path = boost::format("%s%s/%s") 
            % MEDIA_ROOT % "time_shift" % channel["name"].get<string>(); 
        int duration = (60 - tm->tm_min)*60;
        check_path(path.str());
        string file_path = path.str() + "/" + date_fmt.str() + ".mp4"; 
        
#if BY_FFMPEG
        #define FFMPEG_REC_OPTS  " -bsf:a aac_adtstoasc -movflags empty_moov -y -f mp4 "
        auto cmd = boost::format("%s -i udp://%s:%d -t %d -codec copy %s '%s'")
            % FFMPEG % in_multicast % INPUT_PORT % duration % FFMPEG_REC_OPTS % file_path;
        BOOST_LOG_TRIVIAL(info) << cmd.str();
        std::system(cmd.str().c_str());
#else
        gst_task(in_multicast, file_path, duration); 
#endif
    }
}
int main()
{
    vector<thread> pool;
    live_setting live_config;

    CHECK_LICENSE;
    init();

    string time_shift_dir = string(MEDIA_ROOT) + "time_shift";
    if(!boost::filesystem::exists(time_shift_dir)){
        BOOST_LOG_TRIVIAL(info) << "Create " << time_shift_dir;
        boost::filesystem::create_directory(time_shift_dir);
    }
    if(!get_live_config(live_config, "archive")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }

    json silver_channels = json::parse(Mongo::find_mony("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        if(chan["active"] == true && chan["recordTime"] > 0){
            if(chan["inputType"] != live_config.virtual_dvb_id &&
                    chan["inputType"] != live_config.virtual_net_id  )
                pool.emplace_back(start_channel, chan, live_config);
        }
    }
    for(auto& t : pool)
        t.join();
    while(true) this_thread::sleep_for(chrono::seconds(100));
    BOOST_LOG_TRIVIAL(info) << "End!";
    return 0;
} 
