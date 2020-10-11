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
#include <boost/log/trivial.hpp>
#include "gst.hpp"
#include <thread>
#include <mutex>
#include <iostream>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#define FPS 2
#define DELAY (1000/FPS)
#define WIDTH 1280
#define HEIGHT 720
#define FRAME_SIZE (WIDTH * HEIGHT * 4)
#define WAIT_MILISECOND(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))

using namespace std;

GstClockTime timestamp = 0;
uint8_t* frame = nullptr;
cairo_surface_t* surface = nullptr;
std::mutex frame_mutex;

void init_display(int display_id);
void app_need_data (GstElement *appsrc, guint unused_size, gpointer user_data);
void get_snapshot(GObject *object, GAsyncResult *result, gpointer data);

/*
 *   The Gstreamer main function
 *   Streaming of Web site to udp:://out_multicast:port
 *   
 *   @param web_url: URL of Web site
 *   @param out_multicast : multicast of output stream
 *   @param port: output multicast port numper 
 *
 * */
void gst_convert_web_to_stream(string web_url, string out_multicast, int port)
{
    //web_url = "http://127.0.0.1/s/?mmk&u=1&p=1";
    LOG(info) 
        << "Start " << web_url 
        << " --> udp://" << out_multicast << ":" << port;

    Gst::Data d;
    d.loop      = g_main_loop_new(nullptr, false);
    d.pipeline  = GST_PIPELINE(gst_element_factory_make("pipeline", nullptr));
    frame = (uint8_t* ) malloc(FRAME_SIZE);

    try{
        auto appsrc       = Gst::add_element(d.pipeline, "appsrc"),
             queue_src    = Gst::add_element(d.pipeline, "queue", "queue_src"),
             videoconvert = Gst::add_element(d.pipeline, "videoconvert"),
             x264enc      = Gst::add_element(d.pipeline, "x264enc"),
             h264parse    = Gst::add_element(d.pipeline, "h264parse"),
             mpegtsmux    = Gst::add_element(d.pipeline, "mpegtsmux", "mpegtsmux"),
             rndbuffersize  = Gst::add_element(d.pipeline, "rndbuffersize","queue_sink"),
             udpsink      = Gst::add_element(d.pipeline, "udpsink");

        gst_element_link_many(
                appsrc, 
                queue_src, 
                videoconvert, 
                x264enc,
                h264parse,
                mpegtsmux,
                rndbuffersize,
                udpsink,
                nullptr);

        g_object_set (G_OBJECT (appsrc), "caps",
                gst_caps_new_simple ("video/x-raw",
                    "format", G_TYPE_STRING, "RGBx",
                    "width", G_TYPE_INT, WIDTH,
                    "height", G_TYPE_INT, HEIGHT,
                    "framerate", GST_TYPE_FRACTION, FPS, 1,
                    NULL), NULL);
        g_object_set (G_OBJECT (appsrc),
                "stream-type", 0,
                "format", GST_FORMAT_TIME, NULL);

        g_object_set (rndbuffersize,
                "min", 1316,
                "max", 1316,
                nullptr);
        g_object_set (x264enc, "speed-preset", 1, nullptr);

        g_signal_connect (appsrc, "need-data", G_CALLBACK (app_need_data), &d);

        g_object_set(udpsink, 
                "multicast_iface", "lo", 
                "host", out_multicast.c_str() ,
                "port", port,
                "sync", true, 
                nullptr);

        Gst::add_bus_watch(d);
        //Gst::dot_file(d.pipeline, "iptv_archive", 5);

        /////////////////////////////////////////// GTK
        GtkWidget *main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_default_size(GTK_WINDOW(main_window), WIDTH, HEIGHT);

        WebKitWebView *webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
        gtk_container_add(GTK_CONTAINER(main_window), GTK_WIDGET(webView));

        webkit_web_view_load_uri(webView, web_url.c_str());
        bool running = true;
        std::thread snapshot_thread([&](){
                uint32_t n = 0;
                while(running){
                    WAIT_MILISECOND(DELAY);
                    if(++n % 10000 == 0){
                        LOG(debug) << "reload " << web_url;
                        webkit_web_view_reload(webView);
                    } 
                    webkit_web_view_get_snapshot(webView,
                            WEBKIT_SNAPSHOT_REGION_VISIBLE,
                            WEBKIT_SNAPSHOT_OPTIONS_NONE,
                            NULL,
                            get_snapshot,
                            webView);
                    }
                });

        gtk_widget_show_all(main_window);

        /////////////////////////////////////////// Start Pipline
        gst_element_set_state(GST_ELEMENT(d.pipeline), GST_STATE_PLAYING);
        g_main_loop_run(d.loop);
        running = false;
        free(frame);

    }catch(std::exception& e){
        LOG(error) << e.what();
    }
}
void init_display(int display_id)
{
    LOG(trace) << "Init display:" << display_id;
    string display = ":" + to_string(display_id);
    string cmd = "Xvfb " + display + " -screen scrn 1280x720x24   &";
    system(cmd.c_str());
    WAIT_MILISECOND(1000);

    const char* argv_a[] = {"prog", "--display", display.c_str(), nullptr};
    int argc = 3;
    char** argv = (char**) argv_a;
    gtk_init(&argc, &argv);

}
void app_need_data (GstElement *appsrc, guint unused_size, gpointer user_data)
{
    Gst::Data* d = (Gst::Data*) user_data; 
    GstBuffer *buffer;
    GstFlowReturn ret;

    WAIT_MILISECOND(DELAY);
    buffer = gst_buffer_new_allocate (NULL, FRAME_SIZE, NULL);
    {
        std::unique_lock<std::mutex> lock(frame_mutex);
        gst_buffer_fill(buffer, 0, frame, FRAME_SIZE);
    }

    GST_BUFFER_PTS (buffer) = timestamp;
    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, FPS);
    timestamp += GST_BUFFER_DURATION (buffer);
    LOG(trace) << "push buffer";
    g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref (buffer);
    if (ret != GST_FLOW_OK) {
        g_main_loop_quit (d->loop);
    }
}
void get_snapshot(GObject *object, GAsyncResult *result, gpointer data)
{
    WebKitWebView * webview = (WebKitWebView*)data;
    GError* error = NULL;
	surface = webkit_web_view_get_snapshot_finish(webview, result, &error);
    
    LOG(trace) << "Got frame";
    std::unique_lock<std::mutex> lock(frame_mutex);
    uint8_t* f = (uint8_t*) cairo_image_surface_get_data(surface);
    memcpy(frame, f, FRAME_SIZE);
    cairo_surface_destroy(surface);
}
