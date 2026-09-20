/* SP-2 spike client: consumes either transport, verifies ordering, measures
 * per-event latency and CPU. Throwaway. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>

static uint64_t now_ns(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return (uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec; }
static uint32_t get_u32(const unsigned char*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t get_u64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v=(v<<8)|p[i];return v;}
static int conn(const char*path){
    int fd=socket(AF_UNIX,SOCK_STREAM,0);
    struct sockaddr_un a; memset(&a,0,sizeof a); a.sun_family=AF_UNIX;
    snprintf(a.sun_path,sizeof a.sun_path,"%s",path);
    for(int i=0;i<200;i++){ if(!connect(fd,(struct sockaddr*)&a,sizeof a)) return fd;
        struct timespec s={0,10000000}; nanosleep(&s,NULL);}
    perror("connect"); exit(1);
}
static int read_all(int fd,void*b,size_t n){unsigned char*p=b;while(n){ssize_t r=read(fd,p,n);
    if(r<=0)return -1;p+=r;n-=(size_t)r;}return 0;}

int main(int argc,char**argv){
    const char *path="/tmp/sp2.sock"; int framed=1;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--ndjson"))framed=0;
        else if(!strcmp(argv[i],"--framed"))framed=1;
        else if(!strcmp(argv[i],"--path")&&i+1<argc)path=argv[++i];
    }
    char dpath[256]; snprintf(dpath,sizeof dpath,"%s.data",path);
    int fd=conn(path), dfd=-1;
    const char *hello="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"hello\",\"params\":{\"api\":\"1.0\"}}\n";
    write(fd,hello,strlen(hello));

    unsigned char hdr[5], buf[8192]; char line[1024];
    uint64_t json_seq=0, bin_seq=0, events=0, cols=0, order_errors=0, seq_gaps=0;
    double lat_sum=0, lat_max=0; uint64_t start=0;

    if(framed){ /* read hello frame */
        if(read_all(fd,hdr,5)) return 1;
        uint32_t len=get_u32(hdr); if(read_all(fd,buf,len-1)) return 1;
    } else {
        ssize_t r=read(fd,line,sizeof line-1); if(r<=0) return 1;
        dfd=conn(dpath);
    }
    start=now_ns();

    while (1) {
        if (framed) {
            if(read_all(fd,hdr,5)) break;
            uint32_t len=get_u32(hdr); unsigned char type=hdr[4];
            if(len-1 > sizeof buf) break;
            if(read_all(fd,buf,len-1)) break;
            if(type==1){ events++; json_seq=strtoull(strstr((char*)buf,"\"seq\":")+6,NULL,10); }
            else { cols++; bin_seq=get_u64(buf); uint64_t t=get_u64(buf+8);
                   double lat=(now_ns()-t)/1e6; lat_sum+=lat; if(lat>lat_max)lat_max=lat;
                   if(bin_seq!=json_seq) order_errors++;
                   if(bin_seq!=cols) seq_gaps++; }
        } else {
            /* control line */
            ssize_t n=0; char c;
            while(n<(ssize_t)sizeof line-1){ ssize_t r=read(fd,&c,1); if(r<=0){n=-1;break;}
                if(c=='\n')break; line[n++]=c; }
            if(n<0) break; line[n]=0; events++;
            json_seq=strtoull(strstr(line,"\"seq\":")+6,NULL,10);
            unsigned char lh[4];
            if(read_all(dfd,lh,4)) break;
            uint32_t len=get_u32(lh); if(len>sizeof buf) break;
            if(read_all(dfd,buf,len)) break;
            cols++; bin_seq=get_u64(buf); uint64_t t=get_u64(buf+8);
            double lat=(now_ns()-t)/1e6; lat_sum+=lat; if(lat>lat_max)lat_max=lat;
            if(bin_seq!=json_seq) order_errors++;
            if(bin_seq!=cols) seq_gaps++;
        }
    }
    struct rusage ru; getrusage(RUSAGE_SELF,&ru);
    double cpu=ru.ru_utime.tv_sec+ru.ru_utime.tv_usec/1e6+ru.ru_stime.tv_sec+ru.ru_stime.tv_usec/1e6;
    double wall=(now_ns()-start)/1e9;
    printf("client[%s]: events=%llu columns=%llu order_errors=%llu seq_gaps=%llu\n",
        framed?"framed":"ndjson",(unsigned long long)events,(unsigned long long)cols,
        (unsigned long long)order_errors,(unsigned long long)seq_gaps);
    printf("client[%s]: latency mean=%.3f ms max=%.3f ms | cpu=%.4fs over %.2fs = %.2f%% of one core\n",
        framed?"framed":"ndjson", cols?lat_sum/cols:0.0, lat_max, cpu, wall, 100.0*cpu/wall);
    return order_errors? 2 : 0;
}
