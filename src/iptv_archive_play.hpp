#include "glibmm/main.h"
#include "glibmm/refptr.h"
#include "gstreamermm/bus.h"
#include "gstreamermm/message.h"
#include "gstreamermm/pipeline.h"
#include <gstreamermm.h>
#include <glibmm.h>
#include <iostream>
using std::string;

class IptvArchive {
    private:
        string media_path;
        string multicast_addr;
        int port;
    public:
        IptvArchive(string media_path, string multicast_addr, int port)
                :media_path(media_path),multicast_addr(multicast_addr),port(port){}
        void do_work();
        ~IptvArchive(){}
};
