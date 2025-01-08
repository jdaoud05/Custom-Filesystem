#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "blocks.h"
#include "inode.h"
#include "slist.h"

#define DIR_NAME_LENGTH 48

typedef struct dir_entry {
    char name[DIR_NAME_LENGTH];
    int inum;
    char _reserved[12];
} dir_entry_t;

void directory_init();
int directory_lookup(inode_t *di, const char *name);
int directory_put(inode_t *di, const char *name, int inum);
int directory_delete(inode_t *di, const char *name);
slist_t *directory_list(inode_t *di);
void print_directory(inode_t *dd);

#endif
