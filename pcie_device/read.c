#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bars.h"
#include "pcie_dev.h"

static inline void print_usage(const char *argv0) {
    printf("USAGE: %s <bar2_file> [addr] [size]\n", argv0);
}

static char buf[WIN_SIZE + 1];

int main(int argc, const char **argv) {
    if (argc < 3 || argc > 4) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    enum mf_status stt;
    volatile struct pcie_bar2 *bar2;
    struct mapped_file mf;
    uint32_t addr = 0;
    uint32_t size = WIN_SIZE;

    if ((stt = mf_init(&mf, argv[1])) != MF_OK) goto err;

    bar2 = (volatile struct pcie_bar2 *)mf.base;

    if (argc >= 3) addr = strtol(argv[2], NULL, 0);
    if (argc == 4) size = strtol(argv[3], NULL, 0);

    printf("read(addr=%x, size=%x)\n", addr, size);
    bar2->rd_desc.addr_low = addr;
    bar2->rd_desc.addr_high = 0;
    bar2->rd_desc.size = size;
    set_pcie_bar2_rd_csr_start(bar2);

    while (!get_pcie_bar2_irq_rd_comp(bar2)) usleep(POOLING_DELAY);

    // Я не могу сделать регистр с очисткой после чтения, так что придётся
    // очищать прерывание здесь
    unset_pcie_bar2_irq_rd_comp(bar2);

    memcpy(buf, (void *)bar2->rd_data, size);
    buf[size] = 0;

    printf("read_data:\n%s\n", buf);

    mf_cleanup(&mf);
    return EXIT_SUCCESS;
err:
    switch (stt) {
    case MF_FILE_ERROR:
        fprintf(stderr, "file error: `%s`\n", strerror(errno));
        break;
    case MF_MEM_ERROR: fprintf(stderr, "Out of memory!\n"); break;
    case MF_MMAP_ERROR:
        fprintf(stderr, "mmaping file error: `%s`\n", strerror(errno));
        break;
    default: break;
    }
    mf_cleanup(&mf);
    return EXIT_FAILURE;
}

