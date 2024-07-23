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
int running_requests;

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
    while (queue->queue_size + running_requests >= queue->max_size) {
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
void getargs(int *port, int argc, char *argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
}

void* handle_requests(void* arg) {

}

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    int amount_threads;

    // initialize lock and cond vars
    pthread_mutex_init(&m, NULL);
    pthread_cond_init(&queueEmpty, NULL);
    pthread_cond_init(&queueFull, NULL);

    // parse arguments
    getargs(&port, argc, argv);

    // create socket for the listener
    listenfd = Open_listenfd(port);

    // HW3: Create some threads...
    pthread_t* threads = (pthread_t*)malloc(amount_threads * sizeof(pthread_t));
    for (int i=0; i<amount_threads; i++) {
        pthread_create(&threads[i], NULL, handle_requests, NULL);
    }

    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

    // TODO: after accepting a request, add the connfd to the waiting-requests queue
    //      (the worker threads will handle it with requestHandle(connfd) and close it

	// 
	// HW3: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work. 
	// 
	requestHandle(connfd);

	Close(connfd);
    }

    // TODO: should this be here?
    pthread_mutex_destroy(&m);
    pthread_cond_destroy(&queueEmpty);
    pthread_cond_destroy(&queueFull);
    free(threads);
}


    


 
