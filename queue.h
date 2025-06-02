#include "segel.h"
#include "request.h"
#include "log.h"

typedef struct {
    int connfd;
    struct timeval arrival;
    struct timeval dispatch;
    threads_stats t;
    server_log* log;
} request_object;

typedef struct {
    request_object** arr;
    int max_size;
    int front;
    int back;
    int size;
} queue;

void init_queue(queue* q, int max_size) {
    q->arr = (request_object**) malloc(max_size*sizeof(request_object*));
    q->max_size = max_size;
    q->front = 0;
    q->back = 0;
    q->size = 0;
}

void enqueue(queue* q, request_object* object) {
    if (q->size != q->max_size) {
        q->arr[q->back] = object;
        q->back = (q->back + 1) % q->max_size;
        q->size++;
    }
}

request_object* dequeue(queue* q) {
    if (q->size != 0) {
        request_object* object = q->arr[q->front];
        q->front = (q->front + 1) % q->max_size;
        q->size--;
        return object;
    } else {
        return NULL;
    }
}

int is_empty(queue* q) {
    return q->size == 0;
}

int is_full(queue* q) {
    return q->size == q->max_size;
}

int current_size(queue* q) {
    return q->size;
}

void destroy_queue(queue* q) {
    while (q->size)
    {
        request_object* object = dequeue(&q);
        free(object->t);
        free(object);
    }
    free(q->arr);
}