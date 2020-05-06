#include <iostream>
#define PKT_SIZE     1316
#define SOCK_BUF_SIZE  (PKT_SIZE*4000)
#define  INPUT_PORT 3200
#define  MEDIA_ROOT "/opt/sms/www/iptv/media/"
#define INPUT_MULTICAST 229

struct live_setting {
    int type_id;
    int multicast_class;
    std::string multicast_iface;
};
bool get_live_config(live_setting& cfg, std::string type);
std::string get_multicast(live_setting& config, int channel_id, bool out_multicast=false);
std::string get_content_path(int id);
