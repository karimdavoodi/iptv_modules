#include <iostream>
#include <thread>
#include <catch.hpp>
#include <gst/gst.h>
#include "test_util.hpp"
using namespace std;

void gst_stream_media_file(string media_path, string multicast_addr, int port);
bool gst_capture_udp_in_jpg(string in_multicast, int port, const string pic_path);
void gst_mpegts_crypto(string in_multicast, int port, string out_multicast, 
        bool decrypt, string alg_name, string alg_key);

TEST_CASE("encrypt/decrypt stream"){
    cout << "------------------------------------------ IN SCRAMBLE\n";
    gst_init(NULL, NULL);
    // Generate Stream by file
    std::thread stream_archive([](){
            gst_stream_media_file("data/media.mkv", "229.1.1.2", 3300);
            });
    // Encrypt Stream 
    std::thread stream_encrypt([](){
            gst_mpegts_crypto("229.1.1.2",3300, "229.1.1.3", false ,"AES128", "key123");
            });
    // Decrypt Stream 
    std::thread stream_decrypt([](){
            gst_mpegts_crypto("229.1.1.3",3300, "229.1.1.4", true ,"AES128", "key123");
            });
    // Get Snapshot of Stream
    bool res = gst_capture_udp_in_jpg("229.1.1.4", 3300, "data/snapshot.jpg");
    REQUIRE(res);
    REQUIRE(file_size("data/snapshot.jpg") > 100);

    stream_archive.detach();
    stream_encrypt.detach();
    stream_decrypt.detach();
}
