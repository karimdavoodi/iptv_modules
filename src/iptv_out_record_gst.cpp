#include <thread>
#include "utils.hpp"
#include "gst.hpp"
#include <signal.h>
typedef void (*sighandler_t)(int);

using namespace std;

struct Record_data {
    Gst::Data d;
    Mongo db;
    json channel;
    int maxPerChannel;

    Record_data(){}
};

void remove_old_timeshift(Mongo& db, int maxPerChannel, const string channel_name);
void insert_content_info_db(Mongo &db,json& channel, uint64_t id);

void tsdemux_pad_added(GstElement* object, GstPad* pad, gpointer data)
{
    auto rdata = (Record_data *) data;
    Gst::demux_pad_link_to_muxer(rdata->d.pipeline, pad, "mux", "video", "audio_%u");
        /*
    auto caps_filter = gst_caps_new_any();
    auto caps = gst_pad_query_caps(pad, caps_filter);
    auto caps_struct = gst_caps_get_structure(caps, 0);
    auto pad_type = string(gst_structure_get_name(caps_struct));
    auto caps_str = Gst::caps_string(caps);

    LOG(debug) << Gst::pad_name(pad) << " Caps:" << caps_str;
    GstElement* videoparse = nullptr;
    GstElement* audioparse = nullptr;
    GstElement* audiodecoder = nullptr;
    if(pad_type.find("video/x-h264") != string::npos){
        videoparse = Gst::add_element(rdata->d.pipeline, "h264parse", "", true);
        g_object_set(videoparse, "config-interval", 1, nullptr);
    }else if(pad_type.find("video/mpeg") != string::npos){
        int mpegversion = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
        LOG(debug) << "Mpeg version type:" <<  mpegversion;
        if(mpegversion == 4){
            videoparse = Gst::add_element(rdata->d.pipeline, "mpeg4videoparse", "", true);
            g_object_set(videoparse, "config-interval", 1, nullptr);
        }else{
            videoparse = Gst::add_element(rdata->d.pipeline, "mpegvideoparse", "", true);
        }
    }else if(pad_type.find("audio/mpeg") != string::npos){
        int mpegversion = 1;
        gst_structure_get_int(caps_struct, "mpegversion", &mpegversion);
        LOG(debug) << "Mpeg version type:" <<  mpegversion;
        if(mpegversion == 1){
            audioparse = Gst::add_element(rdata->d.pipeline, "mpegaudioparse", "", true);
        }else if(mpegversion == 2 || mpegversion == 4){
            if(caps_str.find("loas") != string::npos){
                // TODO: decode and encode 
                LOG(warning) << "Add transcoder for latm";
                audiodecoder = Gst::add_element(rdata->d.pipeline, "aacparse", "", true);
                auto decoder = Gst::add_element(rdata->d.pipeline, "avdec_aac_latm", 
                        "", true);
                auto audioconvert = Gst::add_element(rdata->d.pipeline, "audioconvert", "", true);
                auto queue = Gst::add_element(rdata->d.pipeline, "queue", "", true);
                auto lamemp3enc = Gst::add_element(rdata->d.pipeline, "lamemp3enc", "", true);
                audioparse = Gst::add_element(rdata->d.pipeline, "mpegaudioparse", 
                        "", true);
                gst_element_link_many(audiodecoder, decoder, audioconvert, queue, 
                                lamemp3enc, audioparse, nullptr);
                g_object_set(audiodecoder, "disable-passthrough", true, nullptr);
            }else{
                audioparse = Gst::add_element(rdata->d.pipeline, "aacparse", "", true);
            }
        }
    }else if(pad_type.find("audio/x-ac3") != string::npos ||
             pad_type.find("audio/ac3") != string::npos){
            audioparse = Gst::add_element(rdata->d.pipeline, "ac3parse", "", true);
    }else{
        LOG(warning) << "Not support:" << pad_type;
    }
    gst_caps_unref(caps_filter);
    gst_caps_unref(caps);
    // link tsdemux ---> queue --- > parse -->  qtmux
    auto parse = (videoparse != nullptr) ? videoparse : audioparse;
    auto parse_name = Gst::element_name(parse);
    if(parse != nullptr){
        g_object_set(parse, "disable-passthrough", true, nullptr);
        auto queue = Gst::add_element(rdata->d.pipeline, "queue", "", true);
        Gst::zero_queue_buffer(queue);
        if(!Gst::pad_link_element_static(pad, queue, "sink")){
            LOG(error) << "Can't link typefind to queue";
            g_main_loop_quit(rdata->d.loop); return; 
        }

        if(audiodecoder){
            gst_element_link(queue, audiodecoder);
        }else{
            gst_element_link(queue, parse);
        }
        auto mux = gst_bin_get_by_name(GST_BIN(rdata->d.pipeline), "mux");
        string sink_name = (videoparse != nullptr) ? "video" : "audio_%u";
        Gst::element_link_request(parse, "src", mux, sink_name.c_str());
        gst_object_unref(mux);
    }
    */
}
gchararray splitmuxsink_location_cb(GstElement*  splitmux,
        guint fragment_id, gpointer data)
{
    Record_data* rdata = (Record_data *) data;
    uint64_t id = std::chrono::system_clock::now().time_since_epoch().count();
    insert_content_info_db(rdata->db, rdata->channel, id);
    remove_old_timeshift(rdata->db, rdata->maxPerChannel, rdata->channel["name"]);

    string file_path = MEDIA_ROOT "TimeShift/" + to_string(id) + ".mp4"; 
    LOG(trace) << "For " << rdata->channel["name"]
               << " New file: " <<  file_path;
    return  g_strdup(file_path.c_str());
}
bool gst_task(json channel, string in_multicast, int port, int maxPerChannel)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start recode " << in_multicast 
        << " for " << maxPerChannel << " hour(s)"; 

    Record_data rdata;
    rdata.channel = channel;
    rdata.maxPerChannel = maxPerChannel;

    rdata.d.loop      = g_main_loop_new(nullptr, false);
    rdata.d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    try{
        auto udpsrc       = Gst::add_element(rdata.d.pipeline, "udpsrc"),
             queue_src    = Gst::add_element(rdata.d.pipeline, "queue", "queue_src"),
             tsdemux      = Gst::add_element(rdata.d.pipeline, "tsdemux"),
             splitmuxsink = Gst::add_element(rdata.d.pipeline, "splitmuxsink", "mux");

        gst_element_link_many(udpsrc, queue_src, tsdemux, nullptr);

        g_signal_connect(tsdemux, "pad-added", G_CALLBACK(tsdemux_pad_added), &rdata);
        g_signal_connect(splitmuxsink, "format-location", 
                G_CALLBACK(splitmuxsink_location_cb), &rdata);

        g_object_set(udpsrc, "uri", in_multicast.c_str(), nullptr);
        g_object_set(splitmuxsink, 
                "max-size-time", RECORD_DURATION * GST_SECOND,   
                "muxer-factory", "qtmux",
                nullptr);

        Gst::add_bus_watch(rdata.d);

        //Gst::dot_file(rdata.d.pipeline, "iptv_network", 5);
        gst_element_set_state(GST_ELEMENT(rdata.d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(rdata.d.loop);
        return true;

    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
        return false;
    }
}
