
#ifndef INODE_H
#define INODE_H

#include "blocks.h"

typedef struct inode {
    int refs;
    int mode;
    int size;
    int blocks[4]; // Array of block numbers instead of single block
} inode_t;

void print_inode(inode_t *node);
inode_t *get_inode(int inum);
int alloc_inode();
void free_inode();
int grow_inode(inode_t *node, int size);
int shrink_inode(inode_t *node, int size);
int inode_get_bnum(inode_t *node, int file_bnum);
int find_inode_by_name(const char* path);
#endif
