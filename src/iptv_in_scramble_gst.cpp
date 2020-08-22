#include <sstream>
#include <thread>
#include <vector>
#include <deque>
#include "utils.hpp"
#include "gst.hpp"
using namespace std;

struct Scramble_data {
    Gst::Data d;
    Scramble_data():d{}{}
};

/*
 *   The Gstreamer main function
 *   Encrypt/Decrypt of input UDP to udp:://multicast:port
 *   
 *   @param in_multicast : multicast of input stream
 *   @param out_multicast : multicast of output stream
 *   @param port: output multicast port numper 
 *   @param alg_name: name of cryptography algorithm: AES128, AES256, BISS
 *   @param alg_key: key of cryptography algorithm
 *
 * */
void gst_task(string in_multicast, int port, string out_multicast, 
        bool decrypt, string alg_name, string alg_key)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) 
        << "Start  " << in_multicast 
        << " --> udp://" << out_multicast << ":" << port;

    Scramble_data tdata;
    tdata.d.loop      = g_main_loop_new(nullptr, false);
    tdata.d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));

    try{
        // Input
        auto udpsrc     = Gst::add_element(tdata.d.pipeline, "udpsrc"),
             queue_src  = Gst::add_element(tdata.d.pipeline, "queue", "queue_src"),
             mpegtscrypt= Gst::add_element(tdata.d.pipeline, "mpegtscrypt"),
             queue_dst  = Gst::add_element(tdata.d.pipeline, "queue", "queue_dst"),
             udpsink    = Gst::add_element(tdata.d.pipeline, "udpsink");

        gst_element_link_many(udpsrc, queue_src, mpegtscrypt, 
                queue_dst, udpsink, nullptr);

        int alg;
        // TODO: only use 'ecb' mode of AES
        if(alg_name == "AES128")      alg = 1; // "aes128_ecb" 
        else if(alg_name == "AES256") alg = 3; // "aes256_ecb"
        else                          alg = 0; // "biss"
        int operation = decrypt ? 0 : 1;
        g_object_set(mpegtscrypt,
                "method", alg,
                "key",    alg_key.c_str(),
                "op",     operation,
                nullptr);

        g_object_set(udpsrc,
                "uri", in_multicast.c_str() ,
                nullptr);

        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                "sync", false,
                nullptr);
        Gst::add_bus_watch(tdata.d);
        gst_element_set_state(GST_ELEMENT(tdata.d.pipeline), GST_STATE_PLAYING);
        Gst::dot_file(tdata.d.pipeline, "iptv_scramble", 9);
        g_main_loop_run(tdata.d.loop);
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
