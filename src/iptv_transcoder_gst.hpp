#include <gstreamermm.h>
#include <glibmm.h>
#include <iostream>
#include "../third_party/json.hpp"
#define TIME_INTERVAL_STATE_SAVE 3000  // msec
using std::string;
using nlohmann::json;

class IptvTranscode {
    private:
        string media_path;
        string multicast_addr;
        int port;
    public:
        IptvTranscode(string media_path, string multicast_addr, int port)
                :media_path(media_path),multicast_addr(multicast_addr),port(port){}
        void do_work(json profile);
        ~IptvTranscode(){}
};
