#include <stddef.h>
#include <stdint.h>

enum mf_status {
    MF_OK = 0,
    MF_FILE_ERROR,
    MF_MMAP_ERROR,
    MF_MSYNC_ERROR,
    MF_MEM_ERROR,
};

struct mapped_file {
    uint8_t *base;
    size_t file_size;
    int fd;
};

enum mf_status mf_init(struct mapped_file *ctx, const char *filename);

enum mf_status
mf_sync(struct mapped_file *ctx, uint32_t addr, uint32_t size, int sync_flag);

void mf_cleanup(struct mapped_file *ctx);
