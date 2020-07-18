#include <sstream>
#include <thread>
#include <vector>
#include <deque>
#include "utils.hpp"
#include "gst.hpp"
using namespace std;

#define WAIT_MILISECOND(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))
#define FRAME_SIZE 1316
#define FPS 2
#define CODEWORD_LENGTH 16
#define BISSKEY_LENGTH 6
extern "C" {
// include C functions from <dvbcsa/dvbcsa.h>
    typedef unsigned char		dvbcsa_cw_t[8];
    typedef struct dvbcsa_key_s	dvbcsa_key_t;
    struct dvbcsa_key_s * dvbcsa_key_alloc(void);
    void dvbcsa_key_free(struct dvbcsa_key_s *key);
    void dvbcsa_key_set (const dvbcsa_cw_t cw, struct dvbcsa_key_s *key);
    void dvbcsa_encrypt (const struct dvbcsa_key_s *key,
            unsigned char *data, unsigned int len);
}

struct Key {
    uint8_t				cw[CODEWORD_LENGTH];
    dvbcsa_key_t		*s_csakey[2];
};
struct Unscramble_data {
    Gst::Data d;
    bool start;
    struct Key key;
    mutex buffer_mutex;
    deque<GstBuffer*> stack;

    Unscramble_data():d{},start{false},stack{}{}
};
////////////////////////////////// Decrypt functions
// copy from tsdecrypt
void csa_key_alloc(struct Key& key) {
    key.s_csakey[0] = dvbcsa_key_alloc();
    key.s_csakey[1] = dvbcsa_key_alloc();
}
void csa_key_free(struct Key& key) {
    dvbcsa_key_free(key.s_csakey[0]);
    dvbcsa_key_free(key.s_csakey[1]);
}
int decode_hex_char(char c) {
    if ((c >= '0') && (c <= '9')) return c - '0';
    if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
    if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
    return -1;
}
int decode_hex_string(const char *hex, uint8_t *bin, int asc_len) {
    int i;
    for (i = 0; i < asc_len; i += 2) {
        int n1 = decode_hex_char(hex[i + 0]);
        int n2 = decode_hex_char(hex[i + 1]);
        if (n1 == -1 || n2 == -1)
            return -1;
        bin[i / 2] = (n1 << 4) | (n2 & 0xf);
    }
    return asc_len / 2;
}
bool init_key(string key_str, struct Key& key_s)
{
    if (key_str.size() > 2 && key_str[0] == '0' && key_str[1] == 'x')
        key_str = key_str.substr(2);
    uint8_t *key = key_s.cw;
    // Sometimes the BISS keys are entered with their checksums already calculated
    // (16 symbols, 8 bytes)
    // This is the same as constant cw with the same key for even and odd
    if (key_str.size() == (BISSKEY_LENGTH + 2) * 2) {
        if (decode_hex_string(key_str.c_str(), key, key_str.size()) < 0) {
            LOG(error) << "Invalid hex string for BISS key: " << key_str;
            return false;
        }
    } else {
        // BISS key without checksum (12 symbols, 6 bytes)
        if (key_str.size() != BISSKEY_LENGTH * 2) {
            LOG(error) << "Invalid BISS key len: " << key_str.size();
            return false;
        }
        if (decode_hex_string(key_str.c_str(), key, key_str.size()) < 0) {
            LOG(error) << "Invalid hex string for BISS key: " << key_str;
            return false;
        }
        // Calculate BISS KEY crc
        memmove(key + 4, key + 3, 3);
        key[3] = (uint8_t)(key[0] + key[1] + key[2]);
        key[7] = (uint8_t)(key[4] + key[5] + key[6]);
    }
    // Even and odd keys are the same
    dvbcsa_key_set(key, key_s.s_csakey[0]);
    dvbcsa_key_set(key, key_s.s_csakey[1]);
    LOG(debug) << "Init BISS key";
    return true;
}
// copy from libtsfuncs
uint8_t ts_packet_get_payload_offset(uint8_t *ts_packet) {
    if (ts_packet[0] != 0x47)
        return 0;

    uint8_t adapt_field   = (ts_packet[3] &~ 0xDF) >> 5; // 11x11111
    uint8_t payload_field = (ts_packet[3] &~ 0xEF) >> 4; // 111x1111

    if (!adapt_field && !payload_field) 
        return 0;

    if (adapt_field) {
        uint8_t adapt_len = ts_packet[4];
        if (payload_field && adapt_len > 182) // Validity checks
            return 0;
        if (!payload_field && adapt_len > 183)
            return 0;
        if (adapt_len + 4 > 188) // adaptation field takes the whole packet
            return 0;
        return 4 + 1 + adapt_len; // ts header + adapt_field_len_byte + adapt_field_len
    } else {
        return 4; // No adaptation, data starts directly after TS header
    }
}
void encode_packet(Unscramble_data *d, uint8_t *ts_packet, int key_idx) {
    unsigned int payload_offset = ts_packet_get_payload_offset(ts_packet);
    if(key_idx == 0)  ts_packet[3] |= 2 << 6;   // even key
    else              ts_packet[3] |= 3 << 6;   // odd key
    dvbcsa_encrypt(d->key.s_csakey[key_idx], ts_packet + payload_offset, 
            188 - payload_offset);
}
void unscramble_buffer(GstBuffer* buffer, Unscramble_data* d)
{
    GstMapInfo map;
    buffer = gst_buffer_make_writable(buffer);
    gst_buffer_map(buffer, &map, GST_MAP_READ); 

    uint8_t* s = map.data;
    int i = 0;
    if(false){
        // Find ts packet
        for(i=0; i<map.size - 190; i++){
            if(s[i] == 0x47 && s[i+188] == 0x47) break;
        } 
        if(i)
            LOG(warning) << "currutp packet. find ts at: " << i;
    }
    int scramble =  s[i+3] >> 6;
    if(scramble){
        bool even = true;
        for (int j=i; j<map.size; j += 188) {
            uint8_t *ts_packet = s + j;
            encode_packet(d, ts_packet, even ? 0 : 1);
            even = !even;
        }
    }else{
        LOG(warning) << "Packet is not scramble";
    }

    gst_buffer_unmap(buffer, &map);
}
void app_need_data (GstElement *appsrc, guint unused_size, gpointer data)
{
    Unscramble_data* d = (Unscramble_data*) data;
    GstBuffer *buffer;
    GstFlowReturn ret;

    if(d->start == false){
        WAIT_MILISECOND(100);
        buffer = gst_buffer_new_allocate(NULL, FRAME_SIZE, NULL);
        LOG(trace) << "Make null packet";
    }else{
        while(!d->stack.size()){
            WAIT_MILISECOND(1);
        } 
        std::unique_lock<std::mutex> lock(d->buffer_mutex);
        buffer = d->stack.front();
        d->stack.pop_front();
    }

    unscramble_buffer(buffer, d);
    //GST_BUFFER_PTS (buffer) = timestamp;
    //GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, FPS);
    //timestamp += GST_BUFFER_DURATION (buffer);
    LOG(trace) << "Size:" <<  gst_buffer_get_size(buffer);
    g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref (buffer);
    if (ret != GST_FLOW_OK) {
        g_main_loop_quit (d->d.loop);
    }
}
GstFlowReturn app_new_sample(GstElement *sink, gpointer *data) {

    Unscramble_data* d = (Unscramble_data*) data;
    GstSample *sample;

    g_signal_emit_by_name (sink, "pull-sample", &sample);
    if (!sample) return GST_FLOW_ERROR;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    LOG(trace) << "Size:" <<  gst_buffer_get_size(buffer);
    if(!buffer){
        LOG(error) << "Not buffer";
        gst_sample_unref (sample);
        return GST_FLOW_ERROR;
    } 

    {
        std::unique_lock<std::mutex> lock(d->buffer_mutex);
        d->stack.push_back(gst_buffer_copy_deep(buffer));
    }
    d->start = true;
    WAIT_MILISECOND(1);

    gst_sample_unref (sample);
    return GST_FLOW_OK;
}

void gst_task(string in_multicast, int port, string out_multicast, string key)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) 
        << "Start  " << in_multicast 
        << " --> udp://" << out_multicast << ":" << port;

    Unscramble_data tdata;
    tdata.d.loop      = g_main_loop_new(nullptr, false);
    tdata.d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));

    try{
        memset(&(tdata.key), 0, sizeof(tdata.key));
        csa_key_alloc(tdata.key);
        if(!init_key(key, tdata.key)){
            return;
        }
        // Input
        auto udpsrc     = Gst::add_element(tdata.d.pipeline, "udpsrc"),
             queue_src  = Gst::add_element(tdata.d.pipeline, "queue", "queue_src"),
             appsink    = Gst::add_element(tdata.d.pipeline, "appsink");
        gst_element_link_many(udpsrc, queue_src, appsink, nullptr);
        g_signal_connect (appsink, "new-sample", G_CALLBACK (app_new_sample), &tdata);
        g_object_set (appsink, "emit-signals", true, nullptr);
        g_object_set(udpsrc, "uri", in_multicast.c_str(), nullptr);


        // Output
        auto appsrc    = Gst::add_element(tdata.d.pipeline, "appsrc"),
             queue_dst  = Gst::add_element(tdata.d.pipeline, "queue", "queue_dst"),
             udpsink    = Gst::add_element(tdata.d.pipeline, "udpsink");
        gst_element_link_many(appsrc, queue_dst, udpsink, nullptr);
        g_signal_connect (appsrc, "need-data", G_CALLBACK (app_need_data), &tdata);
        g_object_set (G_OBJECT (appsrc), "caps",
                gst_caps_new_simple ("video/mpegts",
                    "systemstream", G_TYPE_BOOLEAN, true,
                    "packetsize", G_TYPE_INT, 1316,
                    NULL), NULL);
        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                "sync", false,
                nullptr);
        Gst::add_bus_watch(tdata.d);
        gst_element_set_state(GST_ELEMENT(tdata.d.pipeline), GST_STATE_PLAYING);
        Gst::dot_file(tdata.d.pipeline, "iptv_transcoder", 9);
        g_main_loop_run(tdata.d.loop);
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
    csa_key_alloc(tdata.key);
}
