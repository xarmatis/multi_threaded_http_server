#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"
#include "threadpool.h"
#include "httpserver.h" //CHANGE THIS TO HTTPSERVER
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

static void *thread_pool_worker(void *arg) {

    thread_pool_t *pool = (thread_pool_t *) arg;

    while (true) {

        //pthread_mutex_lock(&pool->queue_lock);

        //while (pool->queue_count == 0 && !pool->shutdown) {

        //  pthread_cond_wait(&pool->queue_not_empty, &pool->queue_lock);
        // }
        if (pool->shutdown) {

            //    pthread_mutex_unlock(&pool->queue_lock);
            break;
        }

        void *task;
        queue_pop(pool->task_queue, &task);
        //pool->queue_count -= 1;
        //pthread_mutex_unlock(&pool->queue_lock);

        //handle the task

        if (task) {
            int connfd = (int) (intptr_t) task;
            handle_connection(connfd);

            //close(connfd);
        }
    }

    return NULL;
}

thread_pool_t *thread_pool_create(int num_threads, int queue_size) {

    if (num_threads <= 0 || queue_size <= 0) {

        fprintf(stderr, "Invalid number of threads or queue size\n");
        return NULL;
    }

    thread_pool_t *pool = malloc(sizeof(thread_pool_t));

    if (!pool) {

        perror("failed to allocated thread pool\n");
        return NULL;
    }
    pool->task_queue = queue_new(queue_size);
    if (!pool->task_queue) {

        free(pool);
        fprintf(stderr, "Failed to create task queue\n");
        return NULL;
    }

    pool->queue_count = 0;
    pool->num_threads = num_threads;
    pool->shutdown = false;

    pthread_mutex_init(&pool->queue_lock, NULL);
    pthread_cond_init(&pool->queue_not_empty, NULL);

    pool->threads = malloc(num_threads * sizeof(pthread_t));
    if (!pool->threads) {

        queue_delete(&pool->task_queue);
        pthread_mutex_destroy(&pool->queue_lock);
        pthread_cond_destroy(&pool->queue_not_empty);
        free(pool);
        fprintf(stderr, "Failed to allocate threads\n");
        return NULL;
    }

    //create thread pool workers

    for (int i = 0; i < num_threads; i++) {

        if (pthread_create(&pool->threads[i], NULL, thread_pool_worker, pool) != 0) {

            perror("Failed to create thread");
            thread_pool_destroy(pool);
            return NULL;
        }
    }

    return pool;
}

bool thread_pool_add_task(thread_pool_t *pool, void *connfd) {

    if (!pool)
        return false;

    //pthread_mutex_lock(&pool->queue_lock);

    bool success = queue_push(pool->task_queue, connfd);
    if (success) {
        // pthread_cond_signal(&pool->queue_not_empty);
        pool->queue_count++;
    }
    //pthread_mutex_unlock(&pool->queue_lock);

    return success;
}

void thread_pool_destroy(thread_pool_t *pool) {

    if (!pool)
        return;

    pthread_mutex_lock(&pool->queue_lock);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->queue_not_empty); //wake all workers so they return
    pthread_mutex_unlock(&pool->queue_lock);

    for (int i = 0; i < pool->num_threads; i++) {

        pthread_join(pool->threads[i], NULL);
    }
    free(pool->threads);
    queue_delete(&pool->task_queue);
    pthread_mutex_destroy(&pool->queue_lock);
    pthread_cond_destroy(&pool->queue_not_empty);
    free(pool);
}
