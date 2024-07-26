#ifndef __REQUEST_H__

#include <sys/time.h>

enum OverLoadPolicy {block, dt, dh, bf, dr};

typedef struct Threads_stats {
    int id;
    int stat_req;
    int dynm_req;
    int total_req;
} threads_stats;

struct reqStats {
    int connfd;
    struct timeval req_arrival;
    struct timeval req_dispatch;
    enum OverLoadPolicy policy;
};

void requestHandle(int fd, threads_stats* thread_stats, struct reqStats req_stats);

#endif
