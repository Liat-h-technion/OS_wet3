#include "segel.h"#include "segel.h"
#include "request.h"
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>

// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

typedef struct queue Queue;

pthread_mutex_t m;
pthread_cond_t queueEmpty;
pthread_cond_t queueFull;
pthread_cond_t blockFlush_queueFull;
int running_requests;
Queue waiting_requests_queue;

enum OverLoadPolicy {block, dt, dh, bf, dr};

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

Node* createNode(struct reqStats req_stats){
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->req_stats = req_stats;
    newNode->next = NULL;
    newNode->prev = NULL;
    return newNode;
}

void createQueue(Queue* q, int max){
    q->head = NULL;
    q->tail = NULL;
    q->max_size = max;
    q->queue_size = 0;
}

void enqueue(Queue* queue, struct reqStats req_stats){
    pthread_mutex_lock(&m);
    //Block
    while ( (req_stats.policy == block) && (queue->queue_size + running_requests >= queue->max_size) ) {
        pthread_cond_wait(&queueFull, &m);
    }
    //Drop Tail
    if ( (req_stats.policy == dt) && (queue->queue_size + running_requests >= queue->max_size)) {
        Close(req_stats.connfd);
        pthread_mutex_unlock(&m);
        return;
    }
    //Drop Head
    if ( (req_stats.policy == dh) && (queue->queue_size + running_requests >= queue->max_size) ) {
        Node *node = queue->head;
        if (queue->head != NULL) {
            if (queue->queue_size == 1) {
                queue->head = NULL;
                queue->tail = NULL;
            } else {
                queue->head = queue->head->next;
                queue->head->prev = NULL;
            }
            queue->queue_size--;
            free(node);
        } //TODO: What to do if queue is empty
    }
    //Block Flush
    bool block_flush_waited = false;
    while ( (req_stats.policy == bf) && (queue->queue_size + running_requests >= queue->max_size) ) {
        block_flush_waited = true;
        pthread_cond_wait(&blockFlush_queueFull, &m);
    }
    if (block_flush_waited == true) {
        Close(req_stats.connfd);
        pthread_mutex_unlock(&m);
        return;
    }

    Node* new_node = createNode(req_stats);
    // add new node to end of queue
    if (queue->head == NULL) {
        queue->head = new_node;
    }
    else {
        queue->tail->next = new_node;
        new_node->prev = queue->tail;
    }
    queue->tail = new_node;
    queue->queue_size++;
    pthread_cond_signal(&queueEmpty);
    pthread_mutex_unlock(&m);
}


struct reqStats dequeue(Queue* queue){
    pthread_mutex_lock(&m);
    while (queue->queue_size == 0) {
        pthread_cond_wait(&queueEmpty, &m);
    }
    // remove node from beginning of queue
    Node* node = queue->head;
    if (queue->queue_size == 1) {
        queue->head = NULL;
        queue->tail = NULL;
    }
    else {
        queue->head = queue->head->next;
        queue->head->prev = NULL;
    }
    struct reqStats req_stats = node->req_stats;
    free(node);
    queue->queue_size--;
    running_requests++;

//  FOR NOW THIS IS NOT NEEDED HERE: pthread_cond_signal(&queueFull);
    pthread_mutex_unlock(&m);
    return req_stats;
}

struct reqStats dequeue_from_end(Queue* queue){
    pthread_mutex_lock(&m);
    while (queue->queue_size == 0) {
        pthread_cond_wait(&queueEmpty, &m);
    }
    // remove node from ending of queue
    Node* node = queue->tail;
    if (queue->queue_size == 1) {
        queue->head = NULL;
        queue->tail = NULL;
    }
    else {
        queue->tail = queue->tail->prev;
        queue->tail->next = NULL;
    }
    struct reqStats req_stats = node->req_stats;
    free(node);
    queue->queue_size--;
    running_requests++;

//  FOR NOW THIS IS NOT NEEDED HERE: pthread_cond_signal(&queueFull);
    pthread_mutex_unlock(&m);
    return req_stats;
}


// HW3: Parse the new arguments too
void getargs(int *port, int *worker_threads, int *queue_size, enum OverLoadPolicy* policy, int argc, char *argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
    *worker_threads = atoi((argv[2]));
    if(*worker_threads <= 0){
        exit(1);
    }
    *queue_size = atoi(argv[3]);
    if(*queue_size <= 0){
        exit(1);
    }

    if (strcmp(argv[4], "block") == 0) {
        *policy = block;
    } else if (strcmp(argv[4], "dt") == 0) {
        *policy = dt;
    } else if (strcmp(argv[4], "dh") == 0) {
        *policy = dh;
    } else if (strcmp(argv[4], "bf") == 0) {
        *policy = bf;
    } else if (strcmp(argv[4], "random") == 0) {
        *policy = dr;
    } else {
        exit(1);
    }
}

void* handle_requests(void* thread_id) {
    int i = *(int*)thread_id;
    while(1) {
        struct reqStats req_stats = dequeue(&waiting_requests_queue);
        gettimeofday(&req_stats.req_dispatch, NULL);
        requestHandle(req_stats.connfd);
        Close(req_stats.connfd);
        pthread_mutex_lock(&m);
        running_requests--;
        if (req_stats.policy == block) {
            pthread_cond_signal(&queueFull);
        }
        else if ( (req_stats.policy == bf) && (waiting_requests_queue.queue_size + running_requests == 0)) {
            pthread_cond_signal(&blockFlush_queueFull);
        }
        pthread_mutex_unlock(&m);
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    int amount_threads, queue_size;
    enum OverLoadPolicy policy;
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    // parse arguments
    getargs(&port, &amount_threads, &queue_size, &policy, argc, argv);

    // initialize lock and cond vars
    pthread_mutex_init(&m, NULL);
    pthread_cond_init(&queueEmpty, NULL);
    pthread_cond_init(&queueFull, NULL);
    pthread_cond_init(&blockFlush_queueFull, NULL);

    // create socket for the listener
    listenfd = Open_listenfd(port);

    // initialize empty queue for waiting requests
    createQueue(&waiting_requests_queue, queue_size);

    // HW3: Create some threads...
    pthread_t* threads = (pthread_t*)malloc(amount_threads * sizeof(pthread_t));
    for (int i=0; i<amount_threads; i++) {
        int *thread_id = malloc(sizeof(*thread_id));
        *thread_id = i;
        pthread_create(&threads[i], NULL, handle_requests, (void*)thread_id);
    }

    while (1) {
        clientlen = sizeof(clientaddr);
        struct reqStats request;
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        gettimeofday(&request.req_arrival, NULL);
        request.connfd = connfd;
        request.policy = policy;
        enqueue(&waiting_requests_queue, request);
    }
}


    


 
