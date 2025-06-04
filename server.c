#include "segel.h"
#include "request.h"
#include "log.h"
#include "queue.h"
#include <pthread.h>

typedef struct {
    pthread_cond_t* isEmpty;
    pthread_cond_t* isFull;
    pthread_mutex_t* m;
    queue* requests;
    request_object** running;
    threads_stats t;
    int* count;
    int* signal;
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
    
    thread_args_t* arg = (thread_args_t*)ptr; 
    pthread_cond_t* isEmpty = arg->isEmpty;
    pthread_cond_t* isFull = arg->isFull;
    pthread_mutex_t* m = arg->m;
    int* count = arg->count;
    int* signal = arg->signal;

    request_object** running = arg->running;

    threads_stats t = arg->t;

    while (*signal) {
        pthread_mutex_lock(m);
        while (!(*signal) || is_empty(arg->requests)) {
            if (!(*signal)) {
                pthread_mutex_unlock(m);
                free(t);
                free(arg);
                pthread_exit(NULL);
            }
            pthread_cond_wait(isEmpty, m);
        }
        request_object* object = dequeue(arg->requests);
        (*count)++;
        int index = 0;
        while (running[index] != NULL) {
            index++;
        }
        running[index] = object; 
        gettimeofday(&object->dispatch, NULL);
        pthread_mutex_unlock(m);

        t->total_req++;
        requestHandle(object->connfd, object->arrival, object->dispatch, t, object->log);

        pthread_mutex_lock(m);
        Close(object->connfd); // Close the connection
        free(object);
        running[index] = NULL;
        (*count)--;
        pthread_cond_signal(isFull);
        pthread_mutex_unlock(m);
    }

    free(t);
    free(arg);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    // Create the global server log
    server_log log = create_log();

    pthread_cond_t isEmpty;
    pthread_cond_t isFull;
    pthread_mutex_t m;

    pthread_cond_init(&isEmpty, NULL);
    pthread_cond_init(&isFull, NULL); 
    pthread_mutex_init(&m, NULL);

    int count = 0;
    volatile int signal = 1;

    int listenfd, connfd, port, clientlen, pool_size, max_queue_size;
    struct sockaddr_in clientaddr;

    getargs(&port, &pool_size, &max_queue_size, argc, argv);

    queue requests;
    init_queue(&requests, max_queue_size);

    request_object** running = (request_object**)malloc(pool_size * sizeof(request_object*));
    for (int i = 0; i < pool_size; i++) 
    {
        running[i] = NULL;
    }

    pthread_t* workers = malloc(pool_size * sizeof(pthread_t));
    for (int i = 0; i < pool_size; i++)
    {
        thread_args_t* arg = (thread_args_t*) malloc(sizeof(thread_args_t));
        arg->isEmpty = &isEmpty;
        arg->isFull = &isFull;
        arg->m = &m;
        arg->requests = &requests;
        arg->running = running;
        arg->count = &count;
        arg->signal = &signal;

        threads_stats t = malloc(sizeof(struct Threads_stats));
        t->id = i+1;             // Thread ID (placeholder)
        t->stat_req = 0;       // Static request count
        t->dynm_req = 0;       // Dynamic request count
        t->post_req = 0;
        t->total_req = 0;      // Total request count

        arg->t = t;

        pthread_create(&workers[i], NULL, worker_m, (void*)arg);
    }
    

    listenfd = Open_listenfd(port);
    while (1) {
        pthread_mutex_lock(&m);

        while ((count + current_size(&requests) >= max_queue_size)) {
            pthread_cond_wait(&isFull, &m);
        }

        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        // TODO: HW3 — Record the request arrival time here

        struct timeval arrival, dispatch;
        dispatch.tv_sec = 0; dispatch.tv_usec = 0; // DEMO: dummy timestamps
        gettimeofday(&arrival, NULL);

        request_object* object = (request_object*) malloc(sizeof(request_object));
        object->connfd = connfd;
        object->arrival = arrival;
        object->dispatch = dispatch;
        object->log = &log;

        enqueue(&requests, object);

        pthread_cond_signal(&isEmpty);

        pthread_mutex_unlock(&m);
    }

    // Clean up the server log before exiting
    destroy_log(log);

    // TODO: HW3 — Add cleanup code for thread pool and queue
    destroy_queue(&requests);

    signal = 0;
    pthread_mutex_lock(&m);
    pthread_cond_broadcast(&isEmpty);
    pthread_mutex_unlock(&m);

    for (int i = 0; i < pool_size; i++) {
        pthread_join(workers[i], NULL);
    }
    for (int i = 0; i < pool_size; i++)
    {
        if (running[i] != NULL) {
            Close(running[i]->connfd); // Close the connection
            free(running[i]);
        }
    }
    free(running);
    free(workers);

    pthread_mutex_destroy(&m);
    pthread_cond_destroy(&isEmpty);
    pthread_cond_destroy(&isFull);
}