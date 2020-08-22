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
#include <exception>
#include <thread>
#include "gst.hpp"

using namespace std;
/*
 *   The Gstreamer main function
 *   Relay udp:://in_multicast:port to udp:://out_multicast:port 
 *   
 *   @param in_multicast : multicast of input stream
 *   @param out_multicast : multicast of output stream
 *   @param port: output multicast port numper 
 *
 * */
void gst_task(string  in_multicast, string out_multicast, int port)
{
    in_multicast = "udp://" + in_multicast + ":" + to_string(port);
    LOG(info) << "Start " << in_multicast << " -> udp://" << out_multicast << ":" << port;

    Gst::Data d;
    d.loop      = g_main_loop_new(nullptr, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));

    try{
        auto udpsrc         = Gst::add_element(d.pipeline, "udpsrc"),
             queue          = Gst::add_element(d.pipeline, "queue"),
             rndbuffersize  = Gst::add_element(d.pipeline, "rndbuffersize"),
             udpsink        = Gst::add_element(d.pipeline, "udpsink");

        gst_element_link_many(udpsrc, queue, rndbuffersize, udpsink, nullptr);
        
        g_object_set(udpsrc, "uri", in_multicast.c_str(), nullptr);
        g_object_set(rndbuffersize, "min", 1316, "max", 1316, nullptr);

        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                "sync", true, nullptr);
        
        Gst::add_bus_watch(d);
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
    }catch(std::exception& e){
        LOG(error) << "Exception:" << e.what();
    }
}
