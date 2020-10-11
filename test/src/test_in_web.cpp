#include <iostream>
#include <thread>
#include <catch.hpp>
#include <gst/gst.h>
#include "test_util.hpp"
using namespace std;

bool gst_capture_udp_in_jpg(string in_multicast, int port, const string pic_path);
void init_display(int display_id);
void gst_convert_web_to_stream(string web_url, string out_multicast, int port);

TEST_CASE("in web"){
    cout << "------------------------------------------ IN WEB\n";
    gst_init(NULL, NULL);
    init_display(1);
    // Generate Stream by site
    std::thread stream_web([](){
            gst_convert_web_to_stream("http://127.0.0.1/s/?mmk=hls&u=1&p=1", "229.1.1.2", 3300);
            });
    wait(10000);
    // Get Snapshot of Stream
    bool res = gst_capture_udp_in_jpg("229.1.1.2", 3300, "data/snapshot.jpg");
    REQUIRE(res);
    REQUIRE(file_size("data/snapshot.jpg") > 100);
    stream_web.detach();
}
