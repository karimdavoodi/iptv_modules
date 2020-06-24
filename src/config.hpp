#pragma onc;

#define ERROR            1
#define INFO             1
#define PKT_SIZE         1316
#define SOCK_BUF_SIZE    (PKT_SIZE*4000)
#define INPUT_PORT       3200
#define INPUT_MULTICAST  229
#define EPG_UPDATE_TIME  15*60        // every 15 minute
#define SNAPSHOT_TIMEOUT 30           // 30 seconds

#define MEDIA_ROOT  "/opt/sms/www/iptv/media/"
#define HLS_ROOT    "/opt/sms/tmp/HLS/"
#define FFMPEG      "/usr/bin/ffmpeg -v quiet "
#define LOG(level) BOOST_LOG_TRIVIAL(level) << "[" << __func__ << ":" <<__LINE__ << "] " 
