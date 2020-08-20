#pragma once
#include <iostream>
#include <boost/log/trivial.hpp>
#include "../third_party/json.hpp"
#include "mongo_driver.hpp"
#include "config.hpp"
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
            BOOST_LOG_TRIVIAL(warning) << "Inactive channel " << chan["name"];\
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
    std::string main_iface;
    int virtual_net_id, virtual_dvb_id;
};
int license_capability_bool(const char *var,int *val);
namespace Util {
    int get_systemId(Mongo& db);
    void system(const std::string cmd);
    void wait(int millisecond);
    void boost_log_init(Mongo& db);
    const std::string shell_out(const std::string cmd);
    void exec_shell_loop(const std::string cmd);
    void report_error(Mongo& db, const std::string, int level = 1);
    bool get_live_config(Mongo& db, live_setting& cfg, std::string type);
    const std::string get_multicast(const live_setting& config, int channel_id, 
                                                bool out_multicast=false);
    const std::string get_content_path(Mongo& db, int id);
    void route_add(int multicast_class, std::string nic);
    void init(Mongo& db);
    void check_path(const std::string path);
    const std::string get_file_content(const std::string name);
    const std::pair<int,int> profile_resolution_pair(const std::string p_vsize);
    const std::string profile_resolution(const std::string p_vsize);
    bool check_weektime(Mongo& db, int weektime_id);
    bool chan_in_output(Mongo &db, int chan_id, int chan_type);
    bool chan_in_input(Mongo &db, int chan_id, int chan_type);
}
