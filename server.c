#include "segel.h"
#include "request.h"
#include "log.h"
#include "queue.h"
#include <pthread.h>

typedef struct {
    pthread_cond_t isEmpty;
    pthread_cond_t isFull;
    pthread_mutex_t m;
    queue* requests;
    int count;
} thread_args_t;


//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// Parses command-line arguments
void getargs(int *port, int *pool_size, int *max_queue_size, int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *pool_size = atoi(argv[2]);
    *max_queue_size = atoi(argv[3]);
}

// TODO: HW3 — Initialize thread pool and request queue
// This server currently handles all requests in the main thread.
// You must implement a thread pool (fixed number of worker threads)
// that process requests from a synchronized queue.

void* worker_m(void* ptr) {
    thread_args_t* thread_args = (thread_args_t*)ptr; 
    pthread_cond_t* isEmpty = &thread_args->isEmpty;
    pthread_cond_t* isFull = &thread_args->isFull;
    pthread_mutex_t* m = &thread_args->m;
    int* count = &thread_args->count;

    while (1) {
        pthread_mutex_lock(m);
        while (is_empty(thread_args->requests)) {
            pthread_cond_wait(isEmpty, m);
        }
        request_object* object = dequeue(thread_args->requests);
        (*count)++;
        pthread_mutex_unlock(&m);

        gettimeofday(&object->dispatch, NULL);
        requestHandle(object->connfd, object->arrival, object->dispatch, object->t, object->log);

        free(object->t); // Cleanup
        Close(object->connfd); // Close the connection
        free(object);

        pthread_mutex_lock(m);
        (*count)--;
        pthread_cond_signal(isFull);
        pthread_mutex_unlock(&m);
    }
}

int main(int argc, char *argv[])
{
    // Create the global server log
    server_log log = create_log();

    int listenfd, connfd, port, clientlen, pool_size, max_queue_size;
    struct sockaddr_in clientaddr;

    getargs(&port, &pool_size, &max_queue_size, argc, argv);

    queue requests;
    init_queue(&requests, max_queue_size);

    thread_args_t thread_args;
    pthread_cond_init(&thread_args.isEmpty, NULL);
    pthread_cond_init(&thread_args.isFull, NULL); 
    pthread_mutex_init(&thread_args.m, NULL);
    thread_args.requests = &requests;
    thread_args.count = 0;

    thread_args_t* arg = &thread_args;

    pthread_t* workers = malloc(pool_size * sizeof(pthread_t));
    for (int i = 0; i < pool_size; i++)
    {
        pthread_create(&workers[i], NULL, worker_m, (void*)arg);
    }
    

    listenfd = Open_listenfd(port);
    while (1) {
        pthread_mutex_lock(&thread_args.m);

        while ((thread_args.count + current_size(&requests) >= max_queue_size)) {
            pthread_cond_wait(&thread_args.isFull, &thread_args.m);
        }

        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        // TODO: HW3 — Record the request arrival time here

        struct timeval arrival, dispatch;
        dispatch.tv_sec = 0; dispatch.tv_usec = 0; // DEMO: dummy timestamps
        gettimeofday(&arrival, NULL);

        threads_stats t = malloc(sizeof(struct Threads_stats));
        t->id = 0;             // Thread ID (placeholder)
        t->stat_req = 0;       // Static request count
        t->dynm_req = 0;       // Dynamic request count
        t->total_req = 0;      // Total request count

        request_object* object = (request_object*) malloc(sizeof(request_object));
        object->connfd = connfd;
        object->arrival = arrival;
        object->dispatch = dispatch;
        object->t = t;
        object->log = &log;

        enqueue(&requests, object);

        pthread_cond_signal(&thread_args.isEmpty);

        pthread_mutex_unlock(&thread_args.m);
    }

    // Clean up the server log before exiting
    destroy_log(log);

    // TODO: HW3 — Add cleanup code for thread pool and queue
    destroy_queue(&requests);
    for (int i = 0; i < pool_size; i++)
    {
        pthread_cancel(workers[i]);
    }
    free(workers);
}
