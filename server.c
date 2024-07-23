#include "segel.h"
#include "request.h"
#include <sys/time.h>
#include <pthread.h>

// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

pthread_mutex_t m;
pthread_cond_t queueEmpty;
pthread_cond_t queueFull;

enum OverLoadPolicy {block, dt, dh, bf, dr};

struct reqStats {
    int connfd;
    struct timeval req_arrival;
    struct timeval req_dispatch;
};

typedef struct node {
    struct reqStats req_stats;
    struct node *next;
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
    return newNode;
}

Queue* createQueue(Queue* q, int max){
    q->head = NULL;
    q->tail = NULL;
    q->max_size = max;
    q->queue_size = 0;
}

void enqueue(Queue* queue, struct reqStats req_stats){
    pthread_mutex_lock(&m);
    Node* new_node = createNode(req_stats);
    while (queue->queue_size >= queue->max_size) {
        pthread_cond_wait(&queueFull, &m);
    }
    // add new node to end of queue
    if (queue->head == NULL) {
        queue->head = new_node;
    }
    else {
        queue->tail->next = new_node;
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
    Node* node = queue->tail;
    if (queue->queue_size == 1) {
        queue->head = NULL;
    }
    queue->tail = NULL;
    struct reqStats req_stats = node->req_stats;
    free(node);
    queue->queue_size--;

    pthread_cond_signal(&queueFull);
    pthread_mutex_unlock(&m);
    return req_stats;
}


// HW3: Parse the new arguments too
void getargs(int *port, int *worker_threads, int *queue_size, enum OverLoadPolicy policy, int argc, char *argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
    *worker_threads = atoi((argv[2]));
    if(*worker_threads <= 0){
        fprintf(stderr, "Invalid number of worker threads: %s\n", argv[2]);
        exit(1); //TODO: exit?? Error?
    }
    *queue_size = atoi(argv[3]);
    if(*queue_size <= 0){
        fprintf(stderr, "Invalid queue size: %s\n", argv[3]);
        exit(1);//TODO: exit?? Error?
    }

    if (strcmp(argv[4], "block") == 0) {
        policy = block;
    } else if (strcmp(argv[4], "dt") == 0) {
        policy = dt;
    } else if (strcmp(argv[4], "dh") == 0) {
        policy = dh;
    } else if (strcmp(argv[4], "bf") == 0) {
        policy = bf;
    } else if (strcmp(argv[4], "random") == 0) {
        policy = dr;
    } else {
        fprintf(stderr, "Invalid policy: %s\n", argv[4]);
        exit(1);//TODO: exit?? Error?
    }
}


int main(int argc, char *argv[])
{
    int worker_threads, queue_size;
    enum OverLoadPolicy policy;
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    int running_requests = 0; //TODO: Needed?

    getargs(&port, &worker_threads, &queue_size, policy, argc, argv);

    // 
    // HW3: Create some threads...
    //
    // TODO: create threads in a loop, send them to execute a function that takes the first
    //      request from the waiting-requests queue, moves it to the running-reuqests queue and handles it

    listenfd = Open_listenfd(port);

    // TODO: create an empty queue for waiting-requests and a queue for running-requests
    Queue waiting_requests;
    createQueue(&waiting_requests, queue_size);

    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

    // TODO: after accepting a request, add the connfd to the waiting-requests queue
    //      (the worker threads will handle it with requestHandle(connfd) and close it

    struct reqStats request;
    request.connfd = connfd;
    enqueue(&waiting_requests, request);
	// HW3: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work. 
	// 
	requestHandle(connfd);

	Close(connfd);
    }

}


    


 
