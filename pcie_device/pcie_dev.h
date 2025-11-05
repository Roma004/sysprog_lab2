#pragma once
#include <pthread.h>
#include <stddef.h>

#include "address_lock.h"
#include "bars.h"
#include "mapped_file.h"
#include "socket.h"

enum pcie_dev_status {
    PCIE_DEV_OK = 0,
    PCIE_DEV_FILE_ERROR,
    PCIE_DEV_MMAP_ERROR,
    PCIE_DEV_MSYNC_ERROR,
    PCIE_DEV_MEM_ERROR,
    PCIE_DEV_THREAD_ERROR,
    PCIE_DEV_SOCKET_ERROR,
};

struct pcie_dev {
    struct mapped_file storage_f;
    struct mapped_file bar0_f;
    struct mapped_file bar2_f;
    volatile struct pcie_bar0 *csr;
    volatile struct pcie_bar2 *data;

    struct socket irq_socket;

    pthread_t rd_thread;
    pthread_t wr_thread;
    int stop_flag;
    struct address_lock storage_lock;
};

enum pcie_dev_status pcie_dev_init(
    struct pcie_dev *ctx,
    const char *bar0_filename,
    const char *bar2_filename,
    const char *storage_filename
);

void pcie_dev_cleanup(struct pcie_dev *ctx);
