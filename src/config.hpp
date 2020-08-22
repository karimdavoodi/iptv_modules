#pragma once

#define MEDIA_ROOT  "/opt/sms/www/iptv/media/"
#define HLS_ROOT    "/opt/sms/tmp/HLS/"
#define FFMPEG      "/usr/bin/ffmpeg -v quiet "
#define LOG(level) BOOST_LOG_TRIVIAL(level) << \
                    "\033[0;32m[" << __func__ << ":" <<__LINE__ << "]\033[0m " 

const int PKT_SIZE = 1316;
const int INPUT_PORT = 3200;
const int INPUT_MULTICAST = 229;
const int EPG_UPDATE_TIME = 15*60;    // for iptv_out_epg (second)
const int SNAPSHOT_TIMEOUT = 30;      // for iptv_out_snapshot (second)
const int RECORD_DURATION = 3600;     // for iptv_out_record (second)

