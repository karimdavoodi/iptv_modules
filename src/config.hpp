#pragma onc;

#define ERROR            1
#define INFO             1
#define PKT_SIZE         1316
#define SOCK_BUF_SIZE    (PKT_SIZE*4000)
#define INPUT_PORT       3200
#define HTTP_STREAM_PORT 8004         // for iptv_out_http
#define INPUT_MULTICAST  229
#define EPG_UPDATE_TIME  15*60        // for iptv_out_epg (second)
#define SNAPSHOT_TIMEOUT 30           // for iptv_out_snapshot (second)
#define DEFAULT_WIDTH    1280         // for iptv_in_mixer
#define DEFAULT_HEIGHT   720          // for iptv_in_mixer
#define RECORD_DURATION  3600         // for iptv_out_record (second)

#define MEDIA_ROOT  "/opt/sms/www/iptv/media/"
#define HLS_ROOT    "/opt/sms/tmp/HLS/"
#define FFMPEG      "/usr/bin/ffmpeg -v quiet "
#define LOG(level) BOOST_LOG_TRIVIAL(level) << \
                    "\033[0;32m[" << __func__ << ":" <<__LINE__ << "]\033[0m " 
