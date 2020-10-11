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
        DB_ERROR(db, 1) << "License is empty!";
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
    
    Util::wait_forever();
} 
