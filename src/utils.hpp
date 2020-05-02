#include <iostream>
#define  INPUT_PORT 3200
#define  MEDIA_ROOT "/opt/sms/www/iptv/media/"
struct live_setting {
    int type_id;
    int multicast_class;
    std::string multicast_iface;
};
bool get_live_config(live_setting& cfg, std::string type);
std::string get_multicast(live_setting& config, int channel_id);
std::string get_content_path(int id);
