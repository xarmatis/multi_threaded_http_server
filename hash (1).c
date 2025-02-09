#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include <pthread.h>

hash_table_t *hash_table[TABLE_SIZE] = { NULL };
static pthread_mutex_t hash_table_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned int hash(char *key) {
    unsigned int hash = 0;
    while (*key) {
        hash = (hash << 5) + *key++;
    }
    return hash % TABLE_SIZE;
}
rwlock_t *get_lock_for_uri(char *uri, PRIORITY priority, uint32_t n) {
    unsigned int idx = hash(uri);
    pthread_mutex_lock(&hash_table_mutex);

    hash_table_t *entry = hash_table[idx];
    while (entry) {
        if (strcmp(entry->key, uri) == 0) {
            rwlock_t *found_lock = entry->lock;
            pthread_mutex_unlock(&hash_table_mutex);
            return found_lock;
        }
        entry = entry->next;
    }

    // Create a new lock if not found
    rwlock_t *new_lock = rwlock_new(priority, n);
    if (new_lock == NULL) {
        pthread_mutex_unlock(&hash_table_mutex);
        return NULL; // Handle allocation failure
    }

    hash_table_t *new_entry = malloc(sizeof(hash_table_t));
    if (new_entry == NULL) {
        free(new_lock);
        pthread_mutex_unlock(&hash_table_mutex);
        return NULL; // Handle allocation failure
    }

    new_entry->key = strdup(uri);
    if (new_entry->key == NULL) {
        free(new_entry);
        free(new_lock);
        pthread_mutex_unlock(&hash_table_mutex);
        return NULL; // Handle allocation failure
    }

    new_entry->lock = new_lock;
    new_entry->next = hash_table[idx];
    hash_table[idx] = new_entry;

    pthread_mutex_unlock(&hash_table_mutex);
    return new_lock;
}


void release_lock_for_uri(char *uri) {
    unsigned int idx = hash(uri);
    pthread_mutex_lock(&hash_table_mutex);
    hash_table_t *prev = NULL;
    hash_table_t *entry = hash_table[idx];

    while (entry) {
        if (strcmp(entry->key, uri) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                hash_table[idx] = entry->next;
            }

            rwlock_delete(&entry->lock);
            free(entry->key);
            free(entry);
            pthread_mutex_unlock(&hash_table_mutex);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}
