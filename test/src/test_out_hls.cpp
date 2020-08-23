#include <iostream>
#include <thread>
#include <catch.hpp>
#include <gst/gst.h>
#include <filesystem>
#include "test_util.hpp"
using namespace std;

void gst_stream_media_file(string media_path, string multicast_addr, int port);
void gst_convert_udp_to_hls(string in_multicast, int in_port, string hls_root);

TEST_CASE("convert udp to hls"){
    gst_init(NULL, NULL);
    // Generate Stream by file
    std::thread stream_archive([](){
            gst_stream_media_file("data/media.mkv", "229.1.1.2", 3300);
            });
    // Convert Stream to Stream
    string path = "/tmp/hls";
    std::thread stream_network([path](){
            std::filesystem::create_directory(path);
            gst_convert_udp_to_hls("229.1.1.2", 3300, path);
            });
    cout << "WAIT 15 SEC, TO GENERATE HLS FILES" << endl;
    wait(15000);
    REQUIRE( std::filesystem::exists(path + "/s__00000.ts") );
    REQUIRE( std::filesystem::exists(path + "/p.m3u8") );

    stream_archive.detach();
    stream_network.detach();
}
