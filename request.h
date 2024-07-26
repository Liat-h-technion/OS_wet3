#ifndef __REQUEST_H__

#include <sys/time.h>
#include <stdbool.h>

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

typedef struct node {
    struct reqStats req_stats;
    struct node *next;
    struct node *prev;
} Node;

typedef struct queue{
    Node* head;
    Node* tail;
    int max_size;
    int queue_size;
} Queue;

Queue waiting_requests_queue;

void requestHandle(int fd, threads_stats* thread_stats, struct reqStats req_stats, bool* is_skip, struct reqStats* skipped_req);

#endif
