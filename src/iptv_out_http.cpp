#include <chrono>
#include <ctime>
#include <exception>
#include <iostream>
#include <vector>
#include <thread>
#include <map>
#include "utils.hpp"

using namespace std;
struct ts_buffer {
    int				ts_i; /* buffer pointer */
    uint8_t		   *ts;   /* buffer */
    long			dt;   /* last time than buffer restart */
    int				dl;   /* buffer len in time */
};
struct Channel {
    int  id;
    string name;
    string multicast;
    struct ts_buffer tsb;
};
void http_unicast_server();
int mcast_sock_create(const char *host, int port,int bind_need,int is_local);
void fill_channel_buffer(uint8_t *data,int len,Channel *c);

map<int, Channel*> chan_map;

void start_channel(json channel)
{
    Channel* C = chan_map[channel["_id"]];
    long MAX_CHANNEL_DATA = 3000*PKT_SIZE;
    int sock;
    uint8_t buf[PKT_SIZE+1];
    int  n;
    BOOST_LOG_TRIVIAL(info) << "Start http buffer for " << channel["name"];
    C->tsb.dl = C->tsb.dt = 0;
    C->tsb.ts_i = 0;
    C->tsb.ts = (uint8_t *)malloc(MAX_CHANNEL_DATA+1);
    if(C->tsb.ts == NULL){
        return;
    }
    if((sock = mcast_sock_create(C->multicast.c_str(),INPUT_PORT,1,1))<0){
        return;
    }
    while(true){
        n = read(sock,buf,PKT_SIZE);
        if(n > 0 ){
            fill_channel_buffer(buf,n,C);
        }
    }		
    close(sock);
}
int main()
{
    vector<thread> pool;
    live_setting live_config;

    init();
    signal(SIGPIPE, SIG_IGN);
    if(!get_live_config(live_config, "archive")){
        BOOST_LOG_TRIVIAL(info) << "Error in live config! Exit.";
        return -1;
    }

    json silver_channels = json::parse(Mongo::find("live_output_silver", "{}"));
    for(auto& chan : silver_channels ){
        if(chan["active"] == true && chan["http"] == true){
            Channel *C = new Channel();
            C->id = chan["_id"];
            C->name = chan["name"];
            live_config.type_id = chan["inputType"];
            C->multicast = get_multicast(live_config, chan["inputId"]);
            C->tsb = {};
            chan_map[chan["_id"]] = C;
            
            pool.emplace_back(start_channel, chan);
        }
    }
    http_unicast_server();
    for(auto& t : pool)
        t.join();
    return 0;
} 