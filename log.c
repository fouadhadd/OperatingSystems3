#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "log.h"

// Opaque struct definition
struct Log_Item {
    char* data;
    int len;
    struct Log_Item* next;
};

struct Server_Log {
    // TODO: Implement internal log storage (e.g., dynamic buffer, linked list, etc.)
    int readers_inside, writers_inside, writers_waiting;
    pthread_cond_t read_allowed;
    pthread_cond_t write_allowed;
    pthread_mutex_t global_lock;
    log_item* head;
    log_item* tail;
    int size;
    int num_char;
};

// Creates a new server log instance (stub)
server_log create_log() {
    // TODO: Allocate and initialize internal log structure
    server_log log = (server_log)malloc(sizeof(struct Server_Log));
    log->readers_inside = 0;
    log->writers_inside = 0; 
    log->writers_waiting = 0;
    pthread_cond_init(&log->read_allowed, NULL);
    pthread_cond_init(&log->write_allowed, NULL);
    pthread_mutex_init(&log->global_lock, NULL);
    log->head = NULL;
    log->tail = NULL;
    log->size = 0;
    log->num_char = 0;
    return log;
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
    // TODO: Free all internal resources used by the log
    log_item* current = log->head;
    while (current != NULL) {
        log_item* toDelete = current;
        current = current->next;
        free(toDelete->data);
        free(toDelete);
    }
    pthread_mutex_destroy(&log->global_lock);
    pthread_cond_destroy(&log->read_allowed);
    pthread_cond_destroy(&log->write_allowed);
    free(log);
}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst) {
    // TODO: Return the full contents of the log as a dynamically allocated string
    // This function should handle concurrent access

    pthread_mutex_lock(&log->global_lock);
    while (log->writers_inside > 0 || log->writers_waiting > 0) {
        pthread_cond_wait(&log->read_allowed, &log->global_lock);
    }
    log->readers_inside++;
    pthread_mutex_unlock(&log->global_lock); 

    *dst = (char*)malloc(log->num_char + 1); // Allocate for caller
    int len = 0;
    if (*dst != NULL) {
        log_item* current = log->head;
        int index = 0;
        while (current != NULL) {
            strncpy((*dst + index), current->data, current->len);
            index += current->len;
            current = current->next;
        }
        (*dst)[index] = '\0';
        len = index;
    }

    pthread_mutex_lock(&log->global_lock);
    log->readers_inside--;
    if (log->readers_inside == 0) {
        pthread_cond_signal(&log->write_allowed);
    }
    pthread_mutex_unlock(&log->global_lock);

    return len;
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len) {
    // TODO: Append the provided data to the log
    // This function should handle concurrent access
    pthread_mutex_lock(&log->global_lock);
    log->writers_waiting++;
    while (log->writers_inside + log->readers_inside > 0) {
        pthread_cond_wait(&log->write_allowed, &log->global_lock);
    }
    log->writers_waiting--;
    log->writers_inside++;

    log_item* toAdd = (log_item*) malloc(sizeof(log_item));
    toAdd->data = (char*) malloc(data_len);
    memcpy(toAdd->data, data, data_len);
    
    toAdd->len = data_len;
    toAdd->next = NULL;

    if (log->head == NULL) {
        log->head = toAdd;
        log->tail = toAdd;
    } else {
        log->tail->next = toAdd;
        log->tail = toAdd;
    }

    log->size++;
    log->num_char += data_len;

    log->writers_inside--;

    pthread_cond_signal(&log->write_allowed);
    pthread_cond_broadcast(&log->read_allowed);
    pthread_mutex_unlock(&log->global_lock);
}