#pragma onc;
#include <iostream>
#include <gstreamermm.h>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include "../third_party/json.hpp"
#include "mongo_driver.hpp"
#include "config.hpp"
#define IS_CHANNEL_VALID(chan)                               \
        if(chan["active"].is_null()) {                       \
            BOOST_LOG_TRIVIAL(error) << "Invalid channel!";  \
            continue;                                        \
        }                                                    \
        if(chan["active"] == false) {                        \
            BOOST_LOG_TRIVIAL(warning) << "Inactive channel!";\
            continue;                                        \
        } do{}while(0)

using nlohmann::json;

struct live_setting {
    int type_id;
    int multicast_class;
    std::string multicast_iface;
    int virtual_net_id, virtual_dvb_id;
};
bool get_live_config(live_setting& cfg, std::string type);
std::string get_multicast(live_setting& config, int channel_id, bool out_multicast=false);
std::string get_content_path(int id);
void route_add(int multicast_class, std::string nic);
void init();
void check_path(const std::string path);
