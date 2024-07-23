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

typedef struct node {
    int connfd;
    struct timeval req_arrival;
    struct timeval req_dispatch;
    struct node *next;
} Node;

typedef struct queue{
    struct Node* head;
    struct Node* tail;
    int max_size;
    int queue_size;
} Queue;

Node* createNode(int conn){
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->connfd = conn;
    newNode->next = NULL;
    return newNode;
}

Queue* createQueue(Queue* q, int max){
    q->head = NULL;
    q->tail = NULL;
    q->max_size = max;
    q->queue_size = 0;
}

void enqueue(Queue* q, Node* node){
    pthread_mutex_lock(&m);
    
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


int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    getargs(&port, argc, argv);

    // 
    // HW3: Create some threads...
    //
    // TODO: create threads in a loop, send them to execute a function that takes the first
    //      request from the waiting-requests queue, moves it to the running-reuqests queue and handles it

    listenfd = Open_listenfd(port);

    // TODO: create an empty queue for waiting-requests and a queue for running-requests

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

}


    


 
