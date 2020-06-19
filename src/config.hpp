#pragma onc;
#define PKT_SIZE     1316
#define ERROR 1
#define INFO  1
#define SOCK_BUF_SIZE  (PKT_SIZE*4000)
#define INPUT_PORT 3200
#define MEDIA_ROOT "/opt/sms/www/iptv/media/"
#define HLS_ROOT "/opt/sms/tmp/HLS/"
#define FFMPEG "/usr/bin/ffmpeg -v quiet "
#define INPUT_MULTICAST 229
#define TIME_INTERVAL_STATE_SAVE 3000
#define EPG_UPDATE_TIME  15*60*1000   // every 15 minute
