#include "glibmm/main.h"
#include "glibmm/refptr.h"
#include "gstreamermm/bus.h"
#include "gstreamermm/message.h"
#include "gstreamermm/pipeline.h"
#include <gstreamermm.h>
#include <glibmm.h>
#include <iostream>
#define TIME_INTERVAL_STATE_SAVE 3000  // msec
using std::string;

class Iptv_to_http {
    private:
        string media_path;
        string multicast_addr;
        int port;
    public:
        Iptv_to_http(string media_path, string multicast_addr, int port)
                :media_path(media_path),multicast_addr(multicast_addr),port(port){}
        void do_work();
        ~Iptv_to_http(){}
};
