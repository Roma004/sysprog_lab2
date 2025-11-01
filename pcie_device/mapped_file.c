#include "mapped_file.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MSYNC_MIN 64

enum mf_status mf_init(struct mapped_file *ctx, const char *filename) {
    struct stat st;
    int fd;
    void *mapped_base;

    fd = open(filename, O_RDWR);
    if (fd == -1) return MF_FILE_ERROR;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return MF_FILE_ERROR;
    }

    mapped_base =
        mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (mapped_base == MAP_FAILED) {
        close(fd);
        return MF_MMAP_ERROR;
    }
    ctx->fd = fd;
    ctx->base = mapped_base;
    ctx->file_size = st.st_size;
    return MF_OK;
}

void mf_cleanup(struct mapped_file *ctx) {
    if (ctx->base) munmap(ctx->base, ctx->file_size);
    if (ctx->fd && ctx->fd != -1) close(ctx->fd);
    memset(ctx, 0, sizeof(*ctx));
}

enum mf_status
mf_sync(struct mapped_file *ctx, uint32_t addr, uint32_t size, int sync_flag) {
    if (size < MSYNC_MIN) size = MSYNC_MIN;
    if (msync(ctx->base + addr, size, sync_flag) == -1) return MF_MSYNC_ERROR;
    return MF_OK;
}
