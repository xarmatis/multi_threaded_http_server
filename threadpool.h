#pragma once
#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"
#include "queue.h"
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/stat.h>

typedef struct thread_pool {
    int queue_count;
    queue_t *task_queue;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_not_empty;
    pthread_t *threads; //thread array
    int num_threads;
    bool shutdown;
} thread_pool_t;

thread_pool_t *thread_pool_create(int num_threads, int queue_size);

bool thread_pool_add_task(
    thread_pool_t *pool, void *connfd); //change this and on line 115 back to void*

void thread_pool_destroy(thread_pool_t *pool);
