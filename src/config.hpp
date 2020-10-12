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
#pragma once

#define MEDIA_ROOT  "/opt/sms/www/iptv/media/"
#define HLS_ROOT    "/opt/sms/tmp/HLS/"
#define FFMPEG      "/usr/bin/ffmpeg -v quiet "
#define LOG(level) BOOST_LOG_TRIVIAL(level) << \
                    "\033[0;32m[" << __func__ << ":" <<__LINE__ << "]\033[0m "

#define DB_ERROR(db, level) Error(db, __FILE__, __func__, __LINE__, level)

const bool ENABLE_LICENSE = true;
const int  INPUT_PORT = 3200;
const int  INPUT_MULTICAST = 229;
const int  EPG_UPDATE_TIME = 15*60;    // for iptv_out_epg (second)
const int  SNAPSHOT_TIMEOUT = 30;      // for iptv_out_snapshot (second)
const int  RECORD_DURATION = 3600;     // for iptv_out_record (second)

const int CONTENT_TYPE_LIVE_VIDEO = 1;
const int CONTENT_TYPE_LIVE_AUDIO = 2;
const int CONTENT_TYPE_SNAPSHOT = 10;
const int CONTENT_FORMAT_MKV = 3;
const int CONTENT_FORMAT_JPG = 10;
