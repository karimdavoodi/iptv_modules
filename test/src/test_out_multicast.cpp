#include <iostream>
#include <thread>
#include <catch.hpp>
#include <gst/gst.h>
#include "test_util.hpp"
using namespace std;

void gst_stream_media_file(string media_path, string multicast_addr, int port);
bool gst_capture_udp_in_jpg(string in_multicast, int port, const string pic_path);
void gst_relay_udp_to_udp(string in_multicast, string out_multicast, int port);

TEST_CASE("relay udp to udp"){
    gst_init(NULL, NULL);
    // Generate Stream by file
    std::thread stream_archive([](){
            gst_stream_media_file("data/media.mkv", "229.1.1.2", 3300);
            });
    // Convert Stream to Stream
    std::thread stream_network([](){
            gst_relay_udp_to_udp("229.1.1.2", "229.1.1.3", 3300);
            });
    // Get Snapshot of Stream
    bool res = gst_capture_udp_in_jpg("229.1.1.3", 3300, "data/snapshot.jpg");
    REQUIRE(res);
    REQUIRE(file_size("data/snapshot.jpg") > 100);

    stream_archive.detach();
    stream_network.detach();
}
