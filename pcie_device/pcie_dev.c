#include "pcie_dev.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "address_lock.h"
#include "bars.h"
#include "socket.h"

#define TRY_PCIE_DEV(action, on_error)                 \
    {                                                  \
        enum pcie_dev_status error_status = action;    \
        if (error_status != PCIE_DEV_OK) { on_error; } \
    }

#define TRY_MF(action, on_error) TRY_PCIE_DEV(mf_status_conv(action), on_error)

static inline int send_interrupt(struct pcie_dev *ctx) {
    return socket_send(&ctx->irq_socket, "I\n");
}

static inline enum pcie_dev_status mf_status_conv(enum mf_status stt) {
    switch (stt) {
    case MF_OK: break;
    case MF_FILE_ERROR: return PCIE_DEV_FILE_ERROR;
    case MF_MMAP_ERROR: return PCIE_DEV_MMAP_ERROR;
    case MF_MEM_ERROR: return PCIE_DEV_MEM_ERROR;
    case MF_MSYNC_ERROR: return PCIE_DEV_MSYNC_ERROR;
    }
    return PCIE_DEV_OK;
}

static inline enum pcie_dev_status
pcie_dev_open_csr(struct pcie_dev *ctx, const char *filename) {
    TRY_MF(mf_init(&ctx->bar2_f, filename), return error_status);
    ctx->csr = (volatile struct pcie_bar0 *)ctx->bar2_f.base;
    return PCIE_DEV_OK;
}

static inline enum pcie_dev_status
pcie_dev_open_data(struct pcie_dev *ctx, const char *filename) {
    TRY_MF(mf_init(&ctx->bar2_f, filename), return error_status);
    ctx->data = (volatile struct pcie_bar2 *)ctx->bar2_f.base;
    return PCIE_DEV_OK;
}

static inline enum pcie_dev_status
pcie_dev_open_storage(struct pcie_dev *ctx, const char *filename) {
    TRY_MF(mf_init(&ctx->storage_f, filename), return error_status);
    return PCIE_DEV_OK;
}

static inline int validate_descriptor(
    struct pcie_dev *dev, uint64_t addr, uint32_t size, int is_write
) {
    if (addr >= dev->storage_f.file_size) {
        if (is_write) {
            set_pcie_bar0_rd_status_addr_error(dev->csr);
        } else {
            set_pcie_bar0_rd_status_addr_error(dev->csr);
        }
        return 0;
    }

    if (addr + size > dev->storage_f.file_size) {
        if (is_write) {
            set_pcie_bar0_wr_status_addr_error(dev->csr);
        } else {
            set_pcie_bar0_rd_status_addr_error(dev->csr);
        }
        return 0;
    }

    return 1;
}

#define INTERRUPT(_dev)                                                 \
    if (!send_interrupt(_dev)) {                                        \
        printf("unable to send interrupt (possibly broken socket)!\n"); \
        goto end;                                                       \
    }

static void *rd_thread_func(void *arg) {
    struct pcie_dev *dev = (struct pcie_dev *)arg;

    while (!__atomic_load_n(&dev->stop_flag, __ATOMIC_ACQUIRE)) {
        if (!get_pcie_bar0_rd_ctrl_start(dev->csr)) {
            usleep(POOLING_DELAY);
            continue;
        }

        unset_pcie_bar0_rd_status_comp(dev->csr);
        unset_pcie_bar0_rd_status_addr_error(dev->csr);
        unset_pcie_bar0_rd_status_size_error(dev->csr);

        // получение дескриптора
        uint64_t addr = (uint64_t)dev->csr->rd_desc.addr_low
                      | ((uint64_t)dev->csr->rd_desc.addr_high << 32);
        uint32_t size = dev->csr->rd_desc.size;

        if (size >= WIN_SIZE) size = WIN_SIZE;

        printf("read(addr=0x%lx, size=0x%x)\n", addr, size);

        // проверка дескриптора
        if (!validate_descriptor(dev, addr, size, 0)) {
            INTERRUPT(dev);
            continue;
        }

        // блокировка чтения
        address_lock_rd_lock(&dev->storage_lock, addr, size);

        // копирование данных из памяти в пространство чтения
        memcpy((void *)dev->data->rd_data, dev->storage_f.base + addr, size);

        // разблокировка чтения
        address_lock_rd_unlock(&dev->storage_lock);

        // информаруем о завершении чтения
        unset_pcie_bar0_rd_ctrl_start(dev->csr);
        set_pcie_bar0_rd_status_comp(dev->csr);
        INTERRUPT(dev);
    }
end:
    printf("READ EXIT!\n");
    return NULL;
}

static void *wr_thread_func(void *arg) {
    struct pcie_dev *dev = (struct pcie_dev *)arg;

    while (!__atomic_load_n(&dev->stop_flag, __ATOMIC_ACQUIRE)) {
        if (!get_pcie_bar0_wr_ctrl_start(dev->csr)) {
            usleep(POOLING_DELAY);
            continue;
        }
        // сброс прерываний и csr
        unset_pcie_bar0_wr_status_comp(dev->csr);
        unset_pcie_bar0_wr_status_addr_error(dev->csr);
        unset_pcie_bar0_wr_status_size_error(dev->csr);

        // получение дескриптора
        uint64_t addr = (uint64_t)dev->csr->wr_desc.addr_low
                      | ((uint64_t)dev->csr->wr_desc.addr_high << 32);
        uint32_t size = dev->csr->wr_desc.size;

        if (size >= WIN_SIZE) size = WIN_SIZE;

        printf("write(addr=0x%lx, size=0x%x)\n", addr, size);

        // проверка дескриптора
        if (!validate_descriptor(dev, addr, size, 1)) {
            INTERRUPT(dev);
            continue;
        }

        // блокировка записи
        address_lock_wr_lock(&dev->storage_lock, addr, size);

        // копирование данных из пространства записи в память
        memcpy(dev->storage_f.base + addr, (void *)dev->data->wr_data, size);

        // синхронизация памяти устройства
        mf_sync(&dev->storage_f, addr, size, MS_SYNC);

        // разблокировка записи
        address_lock_wr_unlock(&dev->storage_lock);

        // информаруем о завершении записи
        unset_pcie_bar0_wr_ctrl_start(dev->csr);
        set_pcie_bar0_wr_status_comp(dev->csr);
        INTERRUPT(dev);
    }
end:
    printf("WRITE EXIT!\n");
    return NULL;
}

enum pcie_dev_status pcie_dev_init(
    struct pcie_dev *ctx,
    const char *bar0_filename,
    const char *bar2_filename,
    const char *storage_filename
) {
    enum pcie_dev_status stt;
    memset(ctx, 0, sizeof(*ctx));

    socket_init(&ctx->irq_socket);
    if (!socket_connect(&ctx->irq_socket, "127.0.0.1", 17887))
        return PCIE_DEV_SOCKET_ERROR;

    if (address_lock_init(&ctx->storage_lock) != 0) return PCIE_DEV_MEM_ERROR;

    TRY_PCIE_DEV(pcie_dev_open_csr(ctx, bar0_filename), goto err);
    TRY_PCIE_DEV(pcie_dev_open_data(ctx, bar2_filename), goto err);
    TRY_PCIE_DEV(pcie_dev_open_storage(ctx, storage_filename),
                 stt = error_status;
                 goto err);

    memset((void *)ctx->csr, 0, sizeof(*ctx->csr));
    ctx->csr->disk_size = ctx->storage_f.file_size;

    if (pthread_create(&ctx->rd_thread, NULL, rd_thread_func, ctx) != 0) {
        stt = PCIE_DEV_THREAD_ERROR;
        goto err;
    }

    if (pthread_create(&ctx->wr_thread, NULL, wr_thread_func, ctx) != 0) {
        stt = PCIE_DEV_THREAD_ERROR;
        goto err;
    }

    return PCIE_DEV_OK;

err:
    pcie_dev_cleanup(ctx);
    return stt;
}

void pcie_dev_cleanup(struct pcie_dev *ctx) {
    __atomic_store_n(&ctx->stop_flag, 1, __ATOMIC_RELEASE);

    if (ctx->irq_socket.fd) socket_close(&ctx->irq_socket);

    if (ctx->rd_thread) pthread_join(ctx->rd_thread, NULL);
    if (ctx->wr_thread) pthread_join(ctx->wr_thread, NULL);

    address_lock_cleanup(&ctx->storage_lock);

    mf_cleanup(&ctx->bar0_f);
    mf_cleanup(&ctx->bar2_f);
    mf_cleanup(&ctx->storage_f);
}
