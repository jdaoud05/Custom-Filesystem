#include "inode.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "directory.h"

#define MAX_FILES 256
#define MAX_FILE_SIZE (4 * BLOCK_SIZE)

static inode_t inode_table[MAX_FILES] = {0};

// Get an inode by its index
inode_t* get_inode(int inum) {
    if (inum < 0 || inum >= MAX_FILES) {
        return 0;
    }
    return &inode_table[inum];
}


// Allocate a new inode
int alloc_inode() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (inode_table[i].refs == 0) {
            memset(&inode_table[i], 0, sizeof(inode_t));
            inode_table[i].refs = 1;
            for (int j = 0; j < 4; j++) {
                inode_table[i].blocks[j] = -1;
            }
            return i;
        }
    }
    return -1;
}


// Free an inode
void free_inode(int inum) {
    if (inum < 0 || inum >= MAX_FILES) return;
    inode_t* node = get_inode(inum);
    for (int i = 0; i < 4; i++) {
        if (node->blocks[i] != -1) {
            free_block(node->blocks[i]);
            node->blocks[i] = -1;
        }
    }
    node->refs = 0;
    node->size = 0;
    node->mode = 0;
}


// Grow an inode's size by allocating necessary blocks
int grow_inode(inode_t *node, int size) {
    if (size > MAX_FILE_SIZE) return -EFBIG;
    int needed_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (int i = 0; i < needed_blocks; i++) {
        if (node->blocks[i] == -1) {
            int block = alloc_block();
            if (block == -1) return -ENOSPC;
            node->blocks[i] = block;
        }
    }
    node->size = size;
    return 0;
}

// Shink an inode's size by freeing unnecessary blocks
int shrink_inode(inode_t *node, int size) {
    int needed_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (int i = needed_blocks; i < 4; i++) {
        if (node->blocks[i] != -1) {
            free_block(node->blocks[i]);
            node->blocks[i] = -1;
        }
    }
    node->size = size;
    return 0;
}

// Get the block number for a given file block number
int inode_get_bnum(inode_t *node, int file_bnum) {
    if (file_bnum >= 4) return -1;
    return node->blocks[file_bnum];
}

// Print the details of an inode
void print_inode(inode_t *node) {
    printf("inode {refs: %d, mode: %o, size: %d, blocks: [%d, %d, %d, %d]}\n",
           node->refs, node->mode, node->size, 
           node->blocks[0], node->blocks[1], 
           node->blocks[2], node->blocks[3]);
}

// Find an inode by its name
int find_inode_by_name(const char *path) {
    inode_t *root_inode = get_inode(0);
    return directory_lookup(root_inode, path);
}
