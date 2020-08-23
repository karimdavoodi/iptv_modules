#include <iostream>
#include <thread>
#include <catch.hpp>
#include <gst/gst.h>
#include <filesystem>
#include "test_util.hpp"
using namespace std;

void gst_stream_media_file(string media_path, string multicast_addr, int port);
void gst_convert_udp_to_http(string in_multicast, int in_port, int http_stream_port);
void gst_convert_stream_to_udp(string in_url, string out_multicast, int port);
bool gst_capture_udp_in_jpg(string in_multicast, int port, const string pic_path);

TEST_CASE("convert udp to http"){
    gst_init(NULL, NULL);
    // Convert file to UDP
    std::thread stream_archive([](){
            gst_stream_media_file("data/media.mkv", "229.1.1.2", 3300);
            });
    wait(1000);
    // Convert UDP to HTTP
    std::thread stream_http([](){
            gst_convert_udp_to_http("229.1.1.2", 3300, 3400);
            });
    wait(5000);
    // Convert HTTP to UDP
    std::thread stream_network([](){
            gst_convert_stream_to_udp("http://127.0.0.1:3400/live.ts", 
                    "229.1.1.3",3300);
            });
    wait(2000);
    // Get Snapshot of Stream
    bool res = gst_capture_udp_in_jpg("229.1.1.3", 3300, "data/snapshot.jpg");
    REQUIRE(res);
    REQUIRE(file_size("data/snapshot.jpg") > 100);

    stream_archive.detach();
    stream_network.detach();
    stream_http.detach();
}
