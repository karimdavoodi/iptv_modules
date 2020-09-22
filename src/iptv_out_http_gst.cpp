/*
 * Copyright (c) 2020 Karim, karimdavoodi@gmail.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <thread>
#include "utils.hpp"
#include "gst.hpp"
#include <gst/video/video-event.h>
using namespace std;

/*
 *   The Gstreamer main function
 *   Convert udp:://in_multicast:port to unicat HTTP stream 
 *   
 *   @param in_multicast : multicast of input stream
 *   @param port: output multicast port numper 
 *   @param http_stream_port: the port of unicast HTTP stream 
 *   @param ch_name: the name of channel 
 *
 * */
void gst_convert_udp_to_http(string in_multicast, int port, int http_stream_port)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start " << in_multicast << " -> http://IP:" << http_stream_port
        << "/live.ts";

    Gst::Data d;
    d.loop      = g_main_loop_new(nullptr, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    try{
        auto udpsrc        = Gst::add_element(d.pipeline, "udpsrc"),
             queue_src     = Gst::add_element(d.pipeline, "queue", "queue_src"),
             tcpserversink = Gst::add_element(d.pipeline, "tcpserversink", "tcpserversink");

        gst_element_link_many(udpsrc, queue_src, tcpserversink, nullptr);
        g_object_set(udpsrc, "uri", in_multicast.c_str(), nullptr);
        g_object_set(tcpserversink,
                "sync-method",  3,                   
                "burst-format", 4,                  
                "burst-value",  2000,   //TODO: test for different platforms
                "buffers-min",  2000,
                "timeout",      3 * GST_SECOND,    
                "host",         "0.0.0.0",
                "port",         http_stream_port,
                nullptr);
        g_signal_connect(tcpserversink, "client-added",G_CALLBACK([](){
                    LOG(debug) << "Client Add";
                }), nullptr); 
        g_signal_connect(tcpserversink, "client-removed",G_CALLBACK([](){
                    LOG(debug) << "Client Removed";
                }), nullptr); 
        Gst::add_bus_watch(d);
        //Gst::dot_file(d.pipeline, "iptv_http", 5);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
/*
void client_added(GstElement* object, GObject* arg0, gpointer user_data)
{

}
void client_removed (GstElement* object, gpointer arg0,
        gpointer arg1, gpointer user_data)
{

}
*/
