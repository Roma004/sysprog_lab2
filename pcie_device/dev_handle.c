#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pcie_dev.h"

volatile sig_atomic_t done = 0;

static inline void print_usage(const char *argv0) {
    printf("USAGE: %s <bar2_file> <storage_file>\n", argv0);
}

void term(int signum) { done = 1; }

int main(int argc, const char **argv) {
    if (argc != 4) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    enum pcie_dev_status stt;
    struct pcie_dev dev;
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    // libevent
    // read and write параллельно
    // https://www.man7.org/linux/man-pages/man7/inotify.7.html

    switch (pcie_dev_init(&dev, argv[1], argv[2], argv[3])) {
    case PCIE_DEV_FILE_ERROR:
        fprintf(stderr, "file error: `%s`\n", strerror(errno));
        break;
    case PCIE_DEV_MEM_ERROR: fprintf(stderr, "Out of memory!\n"); break;
    case PCIE_DEV_MMAP_ERROR:
        fprintf(stderr, "mmaping file error: `%s`\n", strerror(errno));
        break;
    case PCIE_DEV_MSYNC_ERROR:
        fprintf(stderr, "msync error!: `%s`\n", strerror(errno));
        break;
    case PCIE_DEV_THREAD_ERROR:
        fprintf(stderr, "thread error!: `%s`\n", strerror(errno));
        break;
    case PCIE_DEV_SOCKET_ERROR:
        fprintf(stderr, "socket error!: `%s`\n", strerror(errno));
        break;
    case PCIE_DEV_OK: goto loop;
    }

    pcie_dev_cleanup(&dev);
    return EXIT_FAILURE;

loop:
    while (!done);
    pcie_dev_cleanup(&dev);
    return EXIT_SUCCESS;
}
