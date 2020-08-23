#include <iostream>
#include <thread>
#include <catch.hpp>
#include <gst/gst.h>
#include "../../third_party/json.hpp"
#include "test_util.hpp"
using namespace std;
using nlohmann::json;

void gst_stream_media_file(string media_path, string multicast_addr, int port);
bool gst_capture_udp_in_jpg(string in_multicast, int port, const string pic_path);
void gst_transcode_of_stream(string in_multicast, int port, 
                    string out_multicast, json& profile);


TEST_CASE("transcode: change by profile"){
    gst_init(NULL, NULL);
    // Generate Stream by file
    std::thread stream_archive([](){
            gst_stream_media_file("data/media.mkv", "229.1.1.2", 3300);
            });
    // Ttranscode
    std::thread stream_network([](){
            json profile;
            profile["preset"] = "fast";
            profile["videoCodec"] = "h264";
            profile["videoSize"] = "FHD";
            profile["videoFps"] = 24;
            profile["videoRate"] = 3000000;
            profile["videoProfile"] = "";
            profile["audioCodec"] = "mp3";
            profile["audioRate"] = 128000;

            gst_transcode_of_stream("229.1.1.2", 3300, "229.1.1.3", profile);
            });
    // Get Snapshot of Stream
    bool res = gst_capture_udp_in_jpg("229.1.1.3", 3300, "data/snapshot.jpg");
    REQUIRE(res);
    REQUIRE(file_size("data/snapshot.jpg") > 100);

    stream_archive.detach();
    stream_network.detach();
}
