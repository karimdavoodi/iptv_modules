#include <iostream>
#include <thread>
#include <catch.hpp>
#include <gst/gst.h>
#include "test_util.hpp"
using namespace std;

void gst_stream_media_file(string media_path, string multicast_addr, int port);
bool gst_capture_udp_in_jpg(string in_multicast, int port, const string pic_path);

TEST_CASE("archive mkv to udp stream"){
    gst_init(NULL, NULL);
    // Generate Stream
    std::thread stream_archive([](){
            gst_stream_media_file("data/media.mkv", "229.1.1.2", 3300);
            });
    // Get Snapshot of Stream
    bool res = gst_capture_udp_in_jpg("229.1.1.2", 3300, "data/snapshot_mkv.jpg");
    REQUIRE(res);
    REQUIRE(file_size("data/snapshot_mkv.jpg") > 100);
    stream_archive.detach();
}
TEST_CASE("archive mp4 to udp stream"){
    gst_init(NULL, NULL);
    // Generate Stream
    std::thread stream_archive([](){
            gst_stream_media_file("data/media.mp4", "229.1.1.1", 3300);
            });
    // Get Snapshot of Stream
    bool res = gst_capture_udp_in_jpg("229.1.1.1", 3300, "data/snapshot_mp4.jpg");
    REQUIRE(res);
    REQUIRE(file_size("data/snapshot_mp4.jpg") > 100);
    stream_archive.detach();
}
