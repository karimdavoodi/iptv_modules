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
void gst_mix_two_udp_stream(string in_multicast1, string in_multicast2, 
                            string out_multicast, int port, json config);


TEST_CASE("mix two sttream"){
    cout << "------------------------------------------ IN MIX\n";
    gst_init(NULL, NULL);
    // Generate 2 Stream by file
    std::thread stream_archive([](){
            gst_stream_media_file("data/media.mkv", "229.1.1.1", 3300);
            });
    std::thread stream_archive2([](){
            gst_stream_media_file("data/media.mp4", "229.1.1.2", 3300);
            });
    // Ttranscode
    std::thread stream_mix([](){
            json profile;
            profile["input1"] = json::object();
            profile["input1"]["useVideo"] = true;
            profile["input1"]["useAudio"] = true;
            profile["input1"]["audioNumber"] = 1;

            profile["input2"] = json::object();
            profile["input2"]["useVideo"] = true;
            profile["input2"]["useAudio"] = true;
            profile["input2"]["audioNumber"] = 1;
            profile["input2"]["whiteTransparent"] = false;
            profile["input2"]["posX"] = 1;
            profile["input2"]["posY"] = 1;
            profile["input2"]["width"] = 300;
            profile["input2"]["height"] = 300;

            profile["output"] = json::object();
            profile["output"]["width"] = 800;
            profile["output"]["height"] = 800;
            profile["output"]["bitrate"] = 3000000;

            gst_mix_two_udp_stream("229.1.1.1", "229.1.1.2", "229.1.1.3", 3300, profile);
            });
    wait(5000);
    // Get Snapshot of Stream
    bool res = gst_capture_udp_in_jpg("229.1.1.3", 3300, "data/snapshot.jpg");
    REQUIRE(res);
    REQUIRE(file_size("data/snapshot.jpg") > 100);

    stream_archive.detach();
    stream_archive2.detach();
    stream_mix.detach();
}
