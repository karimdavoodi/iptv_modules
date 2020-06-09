#pragma onc;
#include <iostream>
#include <gstreamermm.h>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include "../third_party/json.hpp"
#include "mongo_driver.hpp"
#include "config.hpp"
#define ERROR 1
#define INFO  1
#define THE_END                                            \
    BOOST_LOG_TRIVIAL(warning) << "THE END!";              \
    do{                                                    \
        this_thread::sleep_for(chrono::seconds(100));      \
    }while(true)
#define CHECK_LICENSE                                       \
    do{                                                     \
        int check;                                          \
        if(!license_capability_bool("GB_EPG", &check)){      \
            BOOST_LOG_TRIVIAL(error) << "Invalid license!"; \
            return -1;                                      \
        }                                                   \
    }while(0)
#define IS_CHANNEL_VALID(chan)                               \
        if(chan["active"].is_null()) {                       \
            BOOST_LOG_TRIVIAL(error) << "Invalid channel!";  \
            continue;                                        \
        }                                                    \
        if(chan["active"] == false) {                        \
            BOOST_LOG_TRIVIAL(warning) << "Inactive channel!";\
            continue;                                        \
        }                                                    \
        if(!chan["input"].is_null() &&                     \
           !chan["input"].is_number()) {                   \
            BOOST_LOG_TRIVIAL(error) << "Invalid silver channel!";  \
            continue;                                        \
        } do{}while(0)
using nlohmann::json;
struct live_setting {
    int type_id;
    int multicast_class;
    std::string multicast_iface;
    int virtual_net_id, virtual_dvb_id;
};
void boost_log_init();
void exec_shell_loop(const std::string cmd);
void report_error(const std::string, int level = 1);
//void report_channels(const std::string, int level = 1);
//void report_tuners(const std::string, int level = 1);
int license_capability_bool(const char *var,int *val);
bool get_live_config(live_setting& cfg, std::string type);
std::string get_multicast(live_setting& config, int channel_id, bool out_multicast=false);
std::string get_content_path(int id);
void route_add(int multicast_class, std::string nic);
void init();
void check_path(const std::string path);
const std::string get_file_content(const std::string name);
