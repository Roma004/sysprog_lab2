#pragma once
#include <pthread.h>
#include <stdint.h>

struct lock_pair {
    uint64_t addr;
    uint32_t size;
};

struct address_lock {
    pthread_mutex_t mutex;

    struct lock_pair rd_lock;
    struct lock_pair wr_lock;

    struct lock_pair pending_rd;
    struct lock_pair pending_wr;
};

int address_lock_init(struct address_lock *lock);
void address_lock_cleanup(struct address_lock *lock);

void address_lock_rd_lock(
    struct address_lock *lock, uint64_t addr, uint32_t size
);
void address_lock_rd_unlock(struct address_lock *lock);

void address_lock_wr_lock(
    struct address_lock *lock, uint64_t addr, uint32_t size
);
void address_lock_wr_unlock(struct address_lock *lock);
