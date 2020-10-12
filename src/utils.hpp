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
#include <iostream>
#include <string>
#include <boost/log/trivial.hpp>
#include <string_view>
#include "../third_party/json.hpp"
#include "mongo_driver.hpp"
#include "config.hpp"

#define CHECK_LICENSE                                           \
    do{                                                         \
        if(ENABLE_LICENSE){                                     \
            int check;                                          \
            if(!license_capability_bool("GB_EPG", &check)){     \
                BOOST_LOG_TRIVIAL(error) << "Invalid license!"; \
                return -1;                                      \
            }                                                   \
        }                                                       \
    }while(0)

using nlohmann::json;

int license_capability_bool(const char *var,int *val);

struct live_setting {
    int type_id;
    int multicast_class;
    std::string multicast_iface;
    std::string main_iface;
    live_setting():
        type_id(0),
        multicast_class(239),
        multicast_iface("lo")
    {}
};

class Error {
    private:
        Mongo& db;
        std::string message;
        std::string file;
        std::string func;
        int line;
        int level;
    public:
        Error(Mongo& db, 
                const char* file,
                const char* func,
                int line,
                int level)
             :db(db),file(file),func(func),line(line),level(level){}

        template<typename T>
        Error& operator<<(T msg){
            message = message + std::to_string(msg);
            return *this;
        }
        Error& operator<<(const std::string& msg){
            message = message + msg;
            return *this;
        }
        Error& operator<< (const char* msg){
            message = message + msg;
            return *this;
        }
        void report_error();
        ~Error() { report_error(); }
};

namespace Util {

    void wait_forever();
    void create_directory(const std::string path);
    int get_systemId(Mongo& db);
    void system(const std::string cmd);
    void wait(int millisecond);
    void boost_log_init(Mongo& db);
    const std::string shell_out(const std::string cmd);
    void exec_shell_loop(const std::string cmd);
    bool get_live_config(Mongo& db, live_setting& cfg, std::string type);
    const std::string get_multicast(const live_setting& config, int channel_id, 
            bool out_multicast=false);
    const std::string get_content_path(Mongo& db, int id);
    void add_route_by_mask8(int multicast_class, std::string nic);
    void init(Mongo& db);
    bool is_channel_tv(int64_t input_id, int input_type);
    void check_path(const std::string path);
    const std::string get_file_content(const std::string name);
    const std::pair<int,int> profile_resolution_pair(const std::string p_vsize);
    const std::string profile_resolution(const std::string p_vsize);
    bool check_weektime(Mongo& db, int weektime_id);
    bool chan_in_output(Mongo &db, int chan_id, int chan_type);
    bool chan_in_input(Mongo &db, int chan_id, int chan_type);
    void insert_content_info_db(Mongo &db,json& channel, uint64_t id);
    const std::string get_channel_name(int64_t input_id, int input_type);
    bool check_json_validity(Mongo &db, const std::string record_name,json& record, const json target);
}
