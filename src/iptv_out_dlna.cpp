#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include "utils.hpp"
using namespace std;

int main()
{
    Mongo db;

    CHECK_LICENSE;
    LOG(warning) << "Contents are not in filesystem structure. disable dlna!";
    Util::init(db);
    json license = json::parse(db.find_id("system_license", 1));
    if(license["license"].is_null()){
        LOG(error) << "License in empty!";
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
            Util::system("minidlnad -f /opt/sms/tmp/dlna.conf");
        }else{
            LOG(error) << "Can't open dlna config file";
        }
        
    }else{
        LOG(info) << "DLNA dosen't have license.";
    }
    
    THE_END;
} 
