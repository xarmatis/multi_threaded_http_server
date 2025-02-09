#ifndef HASH_H
#define HASH_H

#include "rwlock.h"

#define TABLE_SIZE 100

typedef struct hash_table {
    char *key;
    rwlock_t *lock;
    struct hash_table *next;
} hash_table_t;

extern hash_table_t *hash_table[TABLE_SIZE];

unsigned int hash(char *key);

rwlock_t *get_lock_for_uri(char *uri, PRIORITY priority, uint32_t n);

void release_lock_for_uri(char *uri);

#endif // HASH_H
