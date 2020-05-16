#include <atomic>
#include <boost/log/trivial.hpp>
#include <cctype>
//#define  __USE_GNU
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <iconv.h>
#include <signal.h>
#include <syslog.h>
#include <thread>
#include <mutex>
#include <map>
#include <semaphore.h>
#include <assert.h>
#include <sys/statvfs.h>
#include <netinet/in.h>
#include "utils.hpp"
#define MAX_CHANNEL  1000
#define PKT_SIZE     1316
#define DEFAULT_PORT 3200
#define UNICAST_HTTP_PORT       8001
#define SOCK_BUF_SIZE  (PKT_SIZE*4000)
#define INPUT_UDP_TIMEOUT  20
#define RECORD_SCHEDULING_DAYS 7

using namespace std;
int debug = 10;
void Log(int level, const char *format, ...)
{
    va_list args;
    if(level <= debug){
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}


// int x<::> = <% %>;   === int x[] = {2,3,4};   C17

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

extern std::map<int, Channel*> chan_map;
///////////////////////////////////////////////////////////////////////////
void set_sock_buf_size(int sock,int n,const char *name)
{
    size_t rec_buf = n;

    Log(3,"Set sock send/recv buf size %d for %s\n",n,name);

    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,&rec_buf,sizeof(rec_buf)) < 0) {
        Log(0,"Failed to set SO_RCVBUF:%s\n",strerror(errno));
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF,&rec_buf,sizeof(rec_buf)) < 0) {
        Log(0,"Failed to set SO_SNDBUF:%s\n",strerror(errno));
    }
}
int mcast_sock_create(const char *host, int port,int bind_need,int is_local)
{
    int on = 1;
    struct ip_mreq mreq;
    struct sockaddr_in receiving_from;
    struct sockaddr_in sockaddr;
    struct timeval tv;
    int sock;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        Log(0,"socket(SOCK_DGRAM): %s\n", strerror(errno));
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct hostent *hostinfo = gethostbyname(host);
    if (hostinfo == NULL) {
        Log(0,"gethostbyname error %s", host);
        close(sock);
        return -1; 
    }
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr = *(struct in_addr *)hostinfo->h_addr;

    memcpy(&mreq.imr_multiaddr, &(sockaddr.sin_addr), sizeof(struct in_addr));
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        Log(0,"ERR  : Failed to add IP membership %s\n",host );
        close(sock);
        return -1;
    }

    if(bind_need){
        memset(&receiving_from, 0, sizeof(receiving_from));
        receiving_from.sin_family = AF_INET;
        receiving_from.sin_addr   = sockaddr.sin_addr;
        receiving_from.sin_port   = htons(port);
        if (bind(sock, (struct sockaddr *) &receiving_from, sizeof(receiving_from)) < 0) {
            Log(0,"ERR  : Failed to bind %s\n",host);
            close(sock);
            return -1;
        }
    }
    tv.tv_sec  = INPUT_UDP_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        Log(0,"error in SO_RCVTIMEO: %s\n", strerror(errno));
        close(sock);
        return -1;
    }
    if(is_local){
        int ttl = 0;  
        if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl)) < 0){
            Log(0,"error in IP_MULTICAST_TTL: %s\n", strerror(errno));
            close(sock);
            return -1;
        }
        char loop = 1;
        if(setsockopt( sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop))<0){
            Log(0,"error in IP_MULTICAST_LOOP: %s\n", strerror(errno));
            close(sock);
            return -1;
        }
        struct sockaddr_in bind_addr;
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY); 

        if(setsockopt( sock, IPPROTO_IP, IP_MULTICAST_IF,
                    (void *)&bind_addr.sin_addr.s_addr,
                    sizeof(bind_addr.sin_addr.s_addr))<0){
            Log(0,"error in IP_MULTICAST_IF: %s\n", strerror(errno));
            close(sock);
            return -1;
        }
    }
    set_sock_buf_size(sock, SOCK_BUF_SIZE,"udp");
    Log(3,"udp-socek=%d\n",sock);
    return sock;
}
void send_http_response( int sockfd, int code, const char* reason)
{
    static char msg[1024];
    int msglen;

    assert( (sockfd > 0) && code && reason );
    msg[0] = '\0';
    msglen = snprintf( msg, sizeof(msg) - 1, 
            "HTTP/1.1 %d %s\r\n"
            "Server: MMK\r\n"
            "Content-Type:application/octet-stream\r\n\r\n",
            code, reason );
    if( msglen <= 0 ) return;
    write(sockfd, msg, msglen);
    return;
}
int tcp_sock_opt(int clientfd)
{
    struct timeval tv;
    int  YES = 1;
    memset(&tv,0,sizeof(struct timeval));
    tv.tv_sec  = 10;
    tv.tv_usec = 0;
    if(clientfd <= 0){
        Log(0,"Invalid socket descriptor\n");
        return -1;
    }
    if (setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        Log(0,"error in setsockopt SO_RCVTIMEO\n");
        //return -1;
    }
    if (setsockopt(clientfd, SOL_SOCKET, SO_SNDTIMEO,&tv,sizeof(tv)) < 0) {
        Log(0,"error in setsockopt SO_SNDTIMEO\n");
        //return -1;
    }
    if (0 != setsockopt(clientfd, IPPROTO_TCP,TCP_NODELAY, &YES, sizeof(YES))) {
        Log(0,"error in setsockopt TCP_NODELAY");
        return -1;
    }
    set_sock_buf_size(clientfd, SOCK_BUF_SIZE,"tcp");
    return 1;
}
int write_all(int s, char *buf, int len)
{
    int total = 0;        
    int bytesleft = len; 
    int n;

    while(total < len) {
        n = write(s, buf+total, bytesleft);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    return n==-1?-1:0; 
} 
#define MAX_USER_PARALLEL_REQ 500
#define MAX_USERS   1000
#define HEADER  "HTTP/1.1 200 OK\r\n"                       \
        "Server: Moojafzar IPTV server (IP Unicaster)\r\n"  \
        "Content-Type:application/octet-stream\r\n\r\n"     

long MAX_CHANNEL_DATA = 3000*PKT_SIZE;
int BUFFER_SEC = 10;   
int max_users = MAX_USERS;
std::atomic_int clients_num = 0;
long max_http_channel = 0;
std::mutex lock;
int get_request_chan_addr(char *client_req, int *chan_id)
{
    int  client_id;
    char *p,*tok;
    char *channel_id;
    char tmp[2048];

    *chan_id = -1;
    strncpy(tmp,client_req,2048);
    p         = strtok_r (tmp,"/.",&tok);  
    channel_id   = strtok_r (NULL ," ",&tok);  
    if(channel_id == NULL || !std::isdigit(channel_id[0]))
            return false;
    *chan_id = atoi(channel_id);

    return true;
}
bool get_chan_multicast(int chan_id, char *chan_multicast)
{

    live_setting live_config;

    json silver_channel = json::parse(Mongo::find_id("live_output_silver", chan_id));
    if(silver_channel.is_null()) return false;
    live_config.type_id = silver_channel["inputType"];
    auto in_multicast = get_multicast(live_config, silver_channel["inputId"]);
    strncpy(chan_multicast, in_multicast.c_str(), 20);
    return true;
}
int find_ts(int n,int max,uint8_t *ts)
{
    int i = n;
    if(ts[n] == 0x47 || n == max ) return n;
    for(i=n; i<max-188; i++){
        if(ts[i+0] == 0x47 && ts[i+188] == 0x47) break;
    } 
    Log(1,"find ts at %d:%d\n",n,i);
    return i;
}
void relay_traffic_directly(int s_udp_sock,int c_tcp_sock,int chan_id,const char *client_ip)
{
    int n, w, wn, wi, size, old_size, count;
    uint8_t	  pkt[PKT_SIZE];   

    count = size = old_size = 0;
    while(true)
    {
        n = read(s_udp_sock,pkt,PKT_SIZE);
        if(n == -1 && errno != EAGAIN ){
            Log(2,"(%s) Server read error(%s)!\n",client_ip, strerror(errno));
            break;
        }
        if(n > 0){
            wn = 3;
            wi = 0;
            do{
                w = write(c_tcp_sock, pkt+wi, n-wi);
                if(w>0) wi += w;
                if(w == -1 && errno != EAGAIN ) break;
            }while( --wn > 0 && wi < n);
            // check error
            if(w == -1 && errno != EAGAIN ){
                Log(2,"(%s) Client connection lost!(%s)\n", client_ip, strerror(errno));
                break;
            }
            // disconnect if send buffer not used
            if(++count == 5000 ){
                count = 0;
                if(!ioctl( c_tcp_sock, TIOCOUTQ, &size)){
                    if(size > (SOCK_BUF_SIZE * 0.9)){
                        if(old_size == size){
                            Log(1,"(%s) Send buffer full(%d). discunnecting!\n", 
                                    client_ip, size);
                            break;
                        }
                        old_size = size;
                    } 
                }
            }
        }
    }
}
void relay_traffic(int client_sock,int chan_id,char *client_http_req,struct in_addr client_addr)
{
    char client_ip[20];
    int n,m,i;
    int len;
    int sec;
    int server_sock;
    char *host;
    int  port;
    Channel* c = chan_map[chan_id];
    BOOST_LOG_TRIVIAL(info) << "Start traffic relay to chan id " << chan_id;
    sec = BUFFER_SEC;
    /* set buffer lenght, per client type */
    if(strstr(client_http_req,"Lavf52.104")) /* samsung hotel tv */
        sec = (sec>3)?3:sec;
    else if(strstr(client_http_req,"VLC"))   /* all VLC */
        sec = (sec>0)?0:sec;
    else if(strstr(client_http_req,"MYAGENT"))   /* my peer */
        sec = (sec>0)?0:sec;
    else if(strstr(client_http_req,"9A405") || strstr(client_http_req,"Android"))  /* Iseema  */
        sec = (sec>3)?3:sec;
    else if(strstr(client_http_req,"souphttpsrc"))  /* samsung tizen j6200 */
        sec = (sec>10)?10:sec;
    else if(strstr(client_http_req,"Lavf/57.71.100"))  /* IJK player in Andriod App  */
        sec = (sec>5)?5:sec; /* FIXME: 0 -> 3 */
    //	else if(strstr(client_http_req,"9A405"))  /* NexBox */
    //		sec = (sec>3)?3:sec;

    if(c->tsb.dl <= sec ){
        len = MAX_CHANNEL_DATA;
    }else{ 
        len = MAX_CHANNEL_DATA * sec / c->tsb.dl;
    }
    if(c->tsb.dl == 0 || c->tsb.dl > 10000)
        len = c->tsb.ts_i;

    len -= (len % PKT_SIZE);

    if(sec == 0 || len < 0  || c->tsb.ts == NULL ) len = 0;

    /* Exception: len=0 for SAMSUNG HOTEL TV */
    if(c->tsb.dl>100/*radio*/ && strstr(client_http_req,"Lavf52.104")/*samsung hotel tv*/){
        len = 0;
    }

    strncpy(client_ip,inet_ntoa(client_addr),20);
    if(debug>1){
        Log(3, "Client:%s\n",client_http_req);

        Log(2, "%d(%s->%s): len %d,sec %d,dl %d,ts_i %d,max %ld\n",
                c->id,c->multicast.c_str(),client_ip,len,sec,c->tsb.dl,c->tsb.ts_i,
                MAX_CHANNEL_DATA);
    }

    if(write(client_sock, HEADER, strlen(HEADER)) == -1){
        Log(1,"(%s) Connection lost(1)!\n",client_ip);
        return;
    }
    /* fill start buffer if sec > 0  */
    if(len > 0 && c->tsb.ts != NULL ){
        if(c->tsb.ts_i < len){
            n = MAX_CHANNEL_DATA -  (len - c->tsb.ts_i);
            n = find_ts(n,MAX_CHANNEL_DATA,c->tsb.ts);
            for(i=n; i<MAX_CHANNEL_DATA; i+=PKT_SIZE){
                m = ((MAX_CHANNEL_DATA-i) >= PKT_SIZE)?PKT_SIZE:MAX_CHANNEL_DATA-i;
                if(write(client_sock, c->tsb.ts + i, m) == -1){
                    Log(1,"(%s) Connection lost(2)!\n",client_ip);
                    return;
                }
            }
            i = 0;
        }else{
            i = c->tsb.ts_i - len;
        }
        i = find_ts(i,c->tsb.ts_i,c->tsb.ts);
        for( ; i < c->tsb.ts_i; i+=PKT_SIZE){
            m = ((c->tsb.ts_i - i) >= PKT_SIZE)?PKT_SIZE:c->tsb.ts_i - i;
            if(write(client_sock, c->tsb.ts + i, m) == -1){
                Log(1,"(%s) Connection lost(3)!\n",client_ip);
                return;
            }
        } 
    }
    if((server_sock = mcast_sock_create(c->multicast.c_str(),INPUT_PORT,1,1))<0){
        Log(0,"Can't open socket to channel %s\n",c->name.c_str());
        return;
    }

    Log(1,"HTTP Relay %s(%s:%d) -> %s\n",
            c->name.c_str(),c->multicast.c_str(),INPUT_PORT,client_ip);

    relay_traffic_directly(server_sock,client_sock,chan_id,(const char *)client_ip);

    shutdown(server_sock,SHUT_RDWR); close(server_sock);

    Log(2,"Close traffic %s -> %s\n",c->multicast.c_str(),client_ip);
}
void http_unicast_relay(int clientfd, struct sockaddr_in sock)
{
    int chan_id;
    int n;
    char client_req[2048];
    int client_id;
    char chan_multicast[20];

    signal(SIGPIPE, SIG_IGN);
    BOOST_LOG_TRIVIAL(info) << "Accept Clinet "; 
    tcp_sock_opt(clientfd);

    if((n=read(clientfd, client_req, 2048)) < 0 ){
        send_http_response(clientfd, 500, "Service error. Can't read req" );
        Log(0,"Error reading from socket\n");
        shutdown(clientfd,SHUT_RDWR); close(clientfd);
        return;
    }
    client_req[n] = '\0';

    if(!get_request_chan_addr(client_req, &chan_id)){
        send_http_response(clientfd, 500, "Service error. Can't get chan name" );
        shutdown(clientfd,SHUT_RDWR); close(clientfd);
        return;
    }
    if(chan_map.count(chan_id) == 0){
        send_http_response(clientfd, 500, "Service error. can't find channel" );
        shutdown(clientfd,SHUT_RDWR); close(clientfd);
        return;
    }
    send_http_response( clientfd, 200, "OK" );

    clients_num++;

    relay_traffic(clientfd,chan_id,client_req,sock.sin_addr);

    clients_num--;

    shutdown(clientfd,SHUT_RDWR); close(clientfd);

    return;
}
void http_unicast_server()
{
    int parentfd,tmpfd;
    int optval;
    struct sockaddr_in tmpsock;
    struct http_thread_arg *arg;
    struct sockaddr_in serveraddr;
    socklen_t clientlen;

    BOOST_LOG_TRIVIAL(info) << "Start http server in port " << UNICAST_HTTP_PORT;
    if((parentfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
        Log(0, "Error open http socket:%s\n",  strerror(errno));
        return;
    }
    optval = 1;
    setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int));

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) UNICAST_HTTP_PORT );

    optval = 0;
    while (bind(parentfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0){ 
        if(++optval > 5){
            close(parentfd);
            exit(0);
        }
        Log(0, "Error bind http socket:%s\n",  strerror(errno));
        sleep(5);
    }
    if (listen(parentfd, 20) < 0){
        Log(0, "Error listen http socket:%s\n",  strerror(errno));
        close(parentfd);
        return;
    }
    clientlen = sizeof(serveraddr);
    while (1) {
        tmpfd = accept(parentfd, (struct sockaddr *) &tmpsock, &clientlen);
        if (tmpfd < 0){ 
            Log(0, "Error accept http socket:%s\n",  strerror(errno));
            close(parentfd);
            return;
        }
        if(clients_num <= max_users*MAX_USER_PARALLEL_REQ ){
            std::thread t(http_unicast_relay, tmpfd, tmpsock );
            t.detach();
        }else{
            send_http_response(tmpfd, 500, "Parallel usage error" );
            close(tmpfd);
        }
    }
    close(parentfd);
}
void fill_channel_buffer(uint8_t *data,int len,Channel *c)
{
    long now;
    if(c->tsb.ts == NULL){
        Log(0,"%s:ts == NULL!\n",__func__);
        return;
    }
    if( (c->tsb.ts_i + len) < MAX_CHANNEL_DATA){   
        memcpy(c->tsb.ts + c->tsb.ts_i,data,len);
        c->tsb.ts_i += len;
    }else{
        memcpy(c->tsb.ts + c->tsb.ts_i,data, MAX_CHANNEL_DATA - c->tsb.ts_i );
        len -= (MAX_CHANNEL_DATA - c->tsb.ts_i);
        memcpy(c->tsb.ts, data, len);
        c->tsb.ts_i = len;
        now = time(NULL);
        c->tsb.dl = now - c->tsb.dt;
        c->tsb.dt = now;
    }
}
