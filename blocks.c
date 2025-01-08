#define _GNU_SOURCE
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bitmap.h"
#include "blocks.h"

const int BLOCK_COUNT = 256;
const int BLOCK_SIZE = 4096;
const int NUFS_SIZE = BLOCK_SIZE * BLOCK_COUNT;
const int BLOCK_BITMAP_SIZE = BLOCK_COUNT / 8;

static int blocks_fd = -1;
static void* blocks_base = 0;

int bytes_to_blocks(int bytes) {
    int quo = bytes / BLOCK_SIZE;
    int rem = bytes % BLOCK_SIZE;
    return (rem == 0) ? quo : quo + 1;
}

void blocks_init(const char* image_path) {
    blocks_fd = open(image_path, O_CREAT | O_RDWR, 0644);
    assert(blocks_fd != -1);
    struct stat stats;
    int rv = fstat(blocks_fd, &stats);
    assert(rv == 0);
    if (stats.st_size == 0) {
        rv = ftruncate(blocks_fd, NUFS_SIZE);
        assert(rv == 0);
        blocks_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, blocks_fd, 0);
        assert(blocks_base != MAP_FAILED);
        // Initialize block 0 (bitmaps)
        void* bbm = get_blocks_bitmap();
        memset(bbm, 0, BLOCK_SIZE);
        bitmap_put(bbm, 0, 1);  // Mark block 0 as used
    } else {
        blocks_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, blocks_fd, 0);
        assert(blocks_base != MAP_FAILED);
    }
}

void blocks_free() {
    if (blocks_base) {
        msync(blocks_base, NUFS_SIZE, MS_SYNC);  // Ensure all changes are written
        int rv = munmap(blocks_base, NUFS_SIZE);
        assert(rv == 0);
        blocks_base = 0;
    }
    if (blocks_fd != -1) {
        close(blocks_fd);
        blocks_fd = -1;
    }
}

void* blocks_get_block(int bnum) {
    assert(bnum >= 0 && bnum < BLOCK_COUNT);
    return blocks_base + (BLOCK_SIZE * bnum);
}

void* get_blocks_bitmap() {
    return blocks_get_block(0);
}

void* get_inode_bitmap() {
    uint8_t* block = blocks_get_block(0);
    return (void*)(block + BLOCK_BITMAP_SIZE);
}

int alloc_block() {
    void* bbm = get_blocks_bitmap();
    for (int ii = 1; ii < BLOCK_COUNT; ++ii) {
        if (!bitmap_get(bbm, ii)) {
            bitmap_put(bbm, ii, 1);
            void* block = blocks_get_block(ii);
            memset(block, 0, BLOCK_SIZE);  // Initialize block to zeros
            printf("+ alloc_block() -> %d\n", ii);
            return ii;
        }
    }
    return -1;
}

void free_block(int bnum) {
    printf("+ free_block(%d)\n", bnum);
    void* bbm = get_blocks_bitmap();
    bitmap_put(bbm, bnum, 0);
}
