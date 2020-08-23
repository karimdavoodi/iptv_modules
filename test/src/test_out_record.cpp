#include <iostream>
#include <thread>
#include <catch.hpp>
#include <gst/gst.h>
#include <filesystem>
#include "../../third_party/json.hpp"
#include "test_util.hpp"
using namespace std;
using nlohmann::json;

void gst_stream_media_file(string media_path, string multicast_addr, int port);
bool gst_convert_udp_to_mp4(json, string in_multicast, int port, int maxPerChannel);

TEST_CASE("convert udp to hls"){
    gst_init(NULL, NULL);
    // Generate Stream by file
    std::thread stream_archive([](){
            gst_stream_media_file("data/media.mkv", "229.1.1.2", 3300);
            });
    // Convert Stream to Stream
    string path = "/tmp/hls";
    std::thread stream_network([path](){
            json channel;
            channel["name"] = "test";
            gst_convert_udp_to_mp4(channel, "229.1.1.2", 3300, 10);
            });
    wait(10000);
    REQUIRE( std::filesystem::exists(path + "/p.m3u8") );
    REQUIRE( std::filesystem::exists(path + "/s_00001.ts") );

    stream_archive.detach();
    stream_network.detach();
}
