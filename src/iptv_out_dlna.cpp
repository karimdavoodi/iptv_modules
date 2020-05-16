#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include "utils.hpp"
using namespace std;
void gst_task(string in_multicast, string out_multicast);

void start_channel(json channel, live_setting live_config)
{
}
int main()
{

    init();
    json license = json::parse(Mongo::find_id("system_license", 1));
    if(license["license"].is_null()){
        BOOST_LOG_TRIVIAL(error) << "License in empty!";
        return -1;
    }
    if(license["license"]["Global"]["GB_DLNA"] == true){

        ofstream dlna("/opt/sms/tmp/dlna.conf");
        if(dlna.is_open()){
            dlna << R"(
media_dir = /opt/sms/www/iptv/media/Video
media_dir = /opt/sms/www/iptv/media/Audio
port = 8200
friendly_name = MMK
serial = 5636410
model_number = 6679
album_art_names=Cover.jpg/cover.jpg
album_art_names=AlbumArt.jpg/albumart.jpg/Album.jpg/album.jpg
album_art_names=Folder.jpg/folder.jpg/Thumb.jpg/thumb.jpg
            )" ;
            dlna.close();
            BOOST_LOG_TRIVIAL(info) << "Start DLNA";
            system("minidlnad -f /opt/sms/tmp/dlna.conf");
        }else{
            BOOST_LOG_TRIVIAL(error) << "Can't open dlna config file";
        }
        
    }else{
        BOOST_LOG_TRIVIAL(info) << "DLNA dosen't have license.";
    }
    
    return 0;
} 
