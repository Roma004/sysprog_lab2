#include "address_lock.h"
#include "bar2.h"

#include <stddef.h>
#include <string.h>
#include <unistd.h>

#define ACTIVE_LOCK(_l) ((_l).size != 0)

static inline int overlaps(struct lock_pair a, struct lock_pair b) {
    return (a.addr < b.addr + b.size) && (b.addr < a.addr + a.size);
}

int address_lock_init(struct address_lock *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    return pthread_mutex_init(&ctx->mutex, NULL);
}

void address_lock_cleanup(struct address_lock *ctx) {
    pthread_mutex_destroy(&ctx->mutex);
    memset(ctx, 0, sizeof(*ctx));
}

void address_lock_rd_lock(
    struct address_lock *ctx, uint64_t addr, uint32_t size
) {
    struct lock_pair new_lock = {
        .addr = addr,
        .size = size,
    };
    int locked = 0;
    pthread_mutex_lock(&ctx->mutex);

    if (ACTIVE_LOCK(ctx->wr_lock) && overlaps(new_lock, ctx->wr_lock)) {
        // Если есть конфликт с текущей блокировкой записи - ставим в очередь
        ctx->pending_rd = new_lock;
    } else {
        // если конфликта нет - блокируем
        ctx->rd_lock = new_lock;
        locked = 1;
    }

    pthread_mutex_unlock(&ctx->mutex);

    if (locked) return;

    // Если был конфликт, то дожидаемся, пока поток записи не установит
    // блокировку чтения из очереди
    while (!ACTIVE_LOCK(ctx->rd_lock)) usleep(POOLING_DELAY);
}

void address_lock_rd_unlock(struct address_lock *ctx) {
    pthread_mutex_lock(&ctx->mutex);

    // Снимаем блокировку чтения
    ctx->rd_lock.addr = 0;
    ctx->rd_lock.size = 0;

    // Если есть ожидающая блокировка записи - устанавливаем ее
    if (ACTIVE_LOCK(ctx->pending_wr)) ctx->wr_lock = ctx->pending_wr;

    pthread_mutex_unlock(&ctx->mutex);
}

void address_lock_wr_lock(
    struct address_lock *ctx, uint64_t addr, uint32_t size
) {
    struct lock_pair new_lock = {
        .addr = addr,
        .size = size,
    };
    int locked = 0;
    pthread_mutex_lock(&ctx->mutex);

    if (ACTIVE_LOCK(ctx->rd_lock) && overlaps(new_lock, ctx->rd_lock)) {
        // Если есть конфликт с текущей блокировкой чтения - ставим в очередь
        ctx->pending_wr = new_lock;
    } else {
        // если конфликта нет - блокируем
        ctx->wr_lock = new_lock;
        locked = 1;
    }

    pthread_mutex_unlock(&ctx->mutex);

    if (locked) return;

    // Если был конфликт, то дожидаемся, пока поток чтения не установит
    // блокировку записи из очереди
    while (!ACTIVE_LOCK(ctx->wr_lock)) usleep(POOLING_DELAY);
}

void address_lock_wr_unlock(struct address_lock *ctx) {
    pthread_mutex_lock(&ctx->mutex);

    // Снимаем блокировку записи
    ctx->wr_lock.addr = 0;
    ctx->wr_lock.size = 0;

    // Если есть ожидающая блокировка чтения - устанавливаем ее
    if (ACTIVE_LOCK(ctx->pending_rd)) ctx->rd_lock = ctx->pending_rd;

    pthread_mutex_unlock(&ctx->mutex);
}
