/* SP-2 spike: throwaway daemon comparing two API transport shapes.
 *
 *   --framed  one socket, every message length-prefixed with a type byte
 *             [u32 len][u8 type][payload]   type 1 = JSON, 2 = binary
 *   --ndjson  control plane is newline-delimited JSON on the main socket;
 *             bulk data goes to a second socket (<path>.data), framed
 *             [u32 len][u64 stream][u64 t_ns][payload]
 *
 * Streams synthetic waterfall columns plus JSON events, with sequence
 * numbers so a client can verify ordering. Not production code.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <poll.h>

#define BINS 256

static uint64_t now_ns(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
static void put_u32(unsigned char *p, uint32_t v) {
    p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v;
}
static void put_u64(unsigned char *p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (unsigned char)(v >> (56 - 8*i));
}
static int listen_unix(const char *path) {
    unlink(path);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un a; memset(&a,0,sizeof a); a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s", path);
    if (bind(fd,(struct sockaddr*)&a,sizeof a) || listen(fd,4)) { perror("bind"); exit(1); }
    return fd;
}
static int send_all(int fd, const void *buf, size_t n) {
    const unsigned char *p = buf;
    while (n) { ssize_t w = write(fd,p,n); if (w<=0) return -1; p+=w; n-=(size_t)w; }
    return 0;
}

int main(int argc, char **argv) {
    const char *path = "/tmp/sp2.sock";
    int framed = 1, rate_hz = 20, seconds = 5;
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--ndjson")) framed = 0;
        else if (!strcmp(argv[i],"--framed")) framed = 1;
        else if (!strcmp(argv[i],"--rate") && i+1<argc) rate_hz = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--seconds") && i+1<argc) seconds = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--path") && i+1<argc) path = argv[++i];
    }
    char dpath[256]; snprintf(dpath,sizeof dpath,"%s.data",path);

    int lfd = listen_unix(path);
    int dlfd = framed ? -1 : listen_unix(dpath);
    fprintf(stderr,"sp2_daemon: mode=%s rate=%dHz seconds=%d socket=%s\n",
            framed?"framed":"ndjson", rate_hz, seconds, path);

    int cfd = accept(lfd,NULL,NULL);
    if (cfd < 0) { perror("accept"); return 1; }
    int dfd = -1;

    /* read the hello line/frame, reply, then stream */
    char req[1024]; ssize_t r = read(cfd, req, sizeof req - 1);
    if (r <= 0) return 1;
    req[r] = 0;

    unsigned char out[4096];
    const char *hello = "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":"
                        "{\"api\":\"1.0\",\"daemon\":\"sp2\",\"mode\":\"%s\"}}\n";
    char line[256]; int ln = snprintf(line,sizeof line,hello, framed?"framed":"ndjson");
    if (framed) {
        put_u32(out, (uint32_t)(1 + ln)); out[4] = 1; memcpy(out+5, line, (size_t)ln);
        send_all(cfd, out, 5 + (size_t)ln);
    } else {
        send_all(cfd, line, (size_t)ln);
        /* The data channel is OPTIONAL: a shell client that only wants the
         * control plane must not block the daemon. Wait briefly, then carry on
         * without it. */
        { struct pollfd pf = { dlfd, POLLIN, 0 };
          if (poll(&pf, 1, 300) > 0) dfd = accept(dlfd, NULL, NULL);
          fprintf(stderr, "daemon: data channel %s\n", dfd>=0 ? "attached" : "absent (control only)"); }
    }

    uint64_t seq = 0, bytes = 0, frames = 0;
    uint64_t start = now_ns();
    uint64_t period = 1000000000ull / (uint64_t)rate_hz;
    uint64_t next = start;

    unsigned char col[BINS];
    while ((now_ns() - start) < (uint64_t)seconds * 1000000000ull) {
        uint64_t t = now_ns();
        if (t < next) { struct timespec s = {0, (long)(next - t)}; nanosleep(&s,NULL); }
        next += period;
        seq++;

        /* 1. JSON event announcing the column (control plane) */
        int jn = snprintf(line, sizeof line,
            "{\"event\":\"waterfall.column\",\"seq\":%llu,\"bins\":%d}\n",
            (unsigned long long)seq, BINS);
        /* 2. the column itself (data plane) */
        for (int i = 0; i < BINS; i++)
            col[i] = (unsigned char)((i * 7 + seq * 3) & 0xff);

        if (framed) {
            put_u32(out, (uint32_t)(1 + jn)); out[4] = 1;
            memcpy(out + 5, line, (size_t)jn);
            size_t n1 = 5 + (size_t)jn;
            unsigned char *p = out + n1;
            put_u32(p, (uint32_t)(1 + 16 + BINS)); p[4] = 2;
            put_u64(p + 5, seq); put_u64(p + 13, now_ns());
            memcpy(p + 21, col, BINS);
            size_t n2 = 5 + 16 + BINS;
            if (send_all(cfd, out, n1 + n2)) break;
            bytes += n1 + n2;
        } else {
            if (send_all(cfd, line, (size_t)jn)) break;
            if (dfd < 0) { bytes += (size_t)jn; frames++; continue; }
            put_u32(out, (uint32_t)(16 + BINS));
            put_u64(out + 4, seq); put_u64(out + 12, now_ns());
            memcpy(out + 20, col, BINS);
            if (send_all(dfd, out, 4 + 16 + BINS)) break;
            bytes += (size_t)jn + 4 + 16 + BINS;
        }
        frames++;
    }
    struct rusage ru; getrusage(RUSAGE_SELF, &ru);
    double cpu = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec/1e6
               + ru.ru_stime.tv_sec + ru.ru_stime.tv_usec/1e6;
    double wall = (now_ns() - start) / 1e9;
    fprintf(stderr,"daemon: frames=%llu bytes=%llu wall=%.2fs cpu=%.4fs (%.2f%% of one core) %.1f B/s\n",
            (unsigned long long)frames,(unsigned long long)bytes, wall, cpu,
            100.0*cpu/wall, bytes/wall);
    close(cfd); if (dfd>=0) close(dfd);
    unlink(path); if (!framed) unlink(dpath);
    return 0;
}
