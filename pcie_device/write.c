#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bar2.h"
#include "pcie_dev.h"

static inline void print_usage(const char *argv0) {
    printf("USAGE: %s <bar2_file> <wr_content_file> [addr] [size]\n", argv0);
}

int main(int argc, const char **argv) {
    if (argc < 3 || argc > 5) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    enum mf_status stt;
    volatile struct pcie_bar2 *bar2;
    struct mapped_file mf;
    uint32_t addr = 0;
    uint32_t size = 1024;
    FILE *wr_ct;

    if ((stt = mf_init(&mf, argv[1])) != MF_OK) goto mf_err;
    if ((wr_ct = fopen(argv[2], "rb")) == NULL) {
        fprintf(
            stderr, "unable to open file '%s': `%s`\n", argv[2], strerror(errno)
        );
        goto err;
    }

    bar2 = (volatile struct pcie_bar2 *)mf.base;

    if (argc >= 4) addr = strtol(argv[4], NULL, 0);
    if (argc == 5) size = strtol(argv[5], NULL, 0);

    printf("write(addr=%x, size=%x)\n", addr, size);
    bar2->wr_desc.addr_low = addr;
    bar2->wr_desc.addr_high = 0;
    bar2->wr_desc.size = size;
    if (!fread((void *)bar2->wr_data, 1, size, wr_ct)) {
        fprintf(stderr, "unable to read from file '%s'\n", argv[2]);
        goto err;
    }

    set_pcie_bar2_wr_csr_start(bar2);

    while (!get_pcie_bar2_irq_wr_comp(bar2)) usleep(POOLING_DELAY);

    // Я не могу сделать регистр с очисткой после чтения, так что придётся
    // очищать прерывание здесь
    unset_pcie_bar2_irq_wr_comp(bar2);

    printf("write success!\n");

    fclose(wr_ct);
    mf_cleanup(&mf);
    return EXIT_SUCCESS;
mf_err:
    switch (stt) {
    case MF_FILE_ERROR:
        fprintf(stderr, "file error: `%s`\n", strerror(errno));
        break;
    case MF_MEM_ERROR: fprintf(stderr, "Out of memory!\n"); break;
    case MF_MMAP_ERROR:
        fprintf(stderr, "mmaping file error: `%s`\n", strerror(errno));
        break;
    case MF_MSYNC_ERROR:
        fprintf(stderr, "msync error: `%s`\n", strerror(errno));
        break;
    case MF_OK: printf("false negative break!\n"); break;
    }
err:
    mf_cleanup(&mf);
    fclose(wr_ct);
    return EXIT_FAILURE;
}

