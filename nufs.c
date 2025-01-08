#include <assert.h>
#include <bsd/string.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "blocks.h"
#include "bitmap.h"
#include "inode.h"
#include "directory.h"
#include "slist.h"

// Definitions for limit of file size
#define MAX_FILES 256
#define MAX_FILENAME 255
#define MAX_FILE_SIZE (4 * BLOCK_SIZE)

// Initializes callbacks for fuse operation
void nufs_init_ops(struct fuse_operations *ops);

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
// Note, for this assignment, you can alternatively implement the create
// function
int nufs_mknod(const char *path, mode_t mode, dev_t rdev) {
    // Extracting filename while removing '/'
    const char* filename = (path[0] == '/') ? path + 1 : path;
    // Checks if file same name already exists
    if (find_inode_by_name(filename) != -1) {
        return -EEXIST;
    }
     // Allocates inode for new file
    int inum = alloc_inode();
    // If no space left
    if (inum == -1) {
        return -ENOSPC;
    }
    // Initializes parts of inode
    inode_t* inode = get_inode(inum);
    inode->refs = 1;
    inode->mode = S_IFREG | mode;
    inode->size = 0;
    // Initializes block pointers
    for (int i = 0; i < 4; i++) {
        inode->blocks[i] = -1;
    }
    // Adds file to root directory
    inode_t* root_inode = get_inode(0);
    if (directory_put(root_inode, filename, inum) < 0) {
        free_inode(inum);
        return -ENOSPC;
    }
    return 0;
}

// Gets an object's attributes (type, permissions, size, etc).
// Implementation for: man 2 stat
// This is a crucial function.
int nufs_getattr(const char *path, struct stat *st) {
    memset(st, 0, sizeof(struct stat));
// Return some metadata for the root directory ...
    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755; // directory
        st->st_size = 0;
        st->st_uid = getuid();
        return 0;
    }
    // Looks for inode in requested path
    const char* filename = path + 1;
    int inum = find_inode_by_name(filename);
    // If file not found
    if (inum == -1) {
        return -ENOENT;
    }
    inode_t* inode = get_inode(inum);
    st->st_mode = inode->mode;
    st->st_size = inode->size;
    st->st_uid = getuid();
    st->st_nlink = 1;
    return 0;
}

// Actually write data
int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
    const char* filename = path + 1;
    int inum = find_inode_by_name(filename);
    // If file not found
    if (inum == -1) return -ENOENT;
    inode_t* inode = get_inode(inum);
    // Check if file is at size limit
    if (offset + size > MAX_FILE_SIZE) return -EFBIG;
    // Grow file if necessary
    if (offset + size > inode->size) {
        if (grow_inode(inode, offset + size) < 0) return -ENOSPC;
    }

    // Writes data by block
    size_t bytes_written = 0;
    while (bytes_written < size) {
        int block_index = (offset + bytes_written) / BLOCK_SIZE;
        int block_offset = (offset + bytes_written) % BLOCK_SIZE;
        int block_num = inode_get_bnum(inode, block_index);
        // Allocates a block if necessary
        if (block_num == -1) {
            block_num = alloc_block();
            // If block limit reached
            if (block_num == -1) break;
            inode->blocks[block_index] = block_num;
        }
        // Writes data to block
        void* block = blocks_get_block(block_num);
        size_t block_bytes = (size - bytes_written < BLOCK_SIZE - block_offset) ? size - bytes_written : BLOCK_SIZE - block_offset;
        memcpy(block + block_offset, buf + bytes_written, block_bytes);
        bytes_written += block_bytes;
    }
    // Updates file size if necessary
    inode->size = (offset + bytes_written > inode->size) ? offset + bytes_written : inode->size;
    return bytes_written;
}

// Actually read data
int nufs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
    const char* filename = path + 1;
    int inum = find_inode_by_name(filename);
    // If file not found
    if (inum == -1) return -ENOENT;
    inode_t* inode = get_inode(inum);
    // If  offset is beyond end of file
    if (offset >= inode->size) return 0;
    size_t bytes_read = 0;
    size_t remaining = (offset + size > inode->size) ? inode->size - offset : size;
    // Reads data by block
    while (remaining > 0) {
        int block_index = offset / BLOCK_SIZE;
        int block_offset = offset % BLOCK_SIZE;
        int block_num = inode_get_bnum(inode, block_index);
        // If block is invalid
        if (block_num == -1) break;
        // Reads data from block
        void* block = blocks_get_block(block_num);
        size_t block_bytes = (remaining < BLOCK_SIZE - block_offset) ? remaining : BLOCK_SIZE - block_offset;
        memcpy(buf + bytes_read, block + block_offset, block_bytes);
        bytes_read += block_bytes;
        offset += block_bytes;
        remaining -= block_bytes;
    }
    return bytes_read;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    // Adds directories
    filler(buf, ".", &st, 0);
    filler(buf, "..", &st, 0);
    // Gets root inode
    inode_t* root_inode = get_inode(0);
    slist_t* entries = directory_list(root_inode);
    // Iterates through directory entries and adds it to buffer
    for (slist_t* curr = entries; curr; curr = curr->next) {
        int inum = find_inode_by_name(curr->data);
        if (inum >= 0) {
            inode_t* inode = get_inode(inum);
            st.st_mode = inode->mode;
            st.st_size = inode->size;
            filler(buf, curr->data, &st, 0);
        }
    }
    slist_free(entries);
    return 0;
}

// Deletes file 
int nufs_unlink(const char *path) {
    const char* filename = path + 1;
    inode_t* root_inode = get_inode(0);
    int inum = directory_lookup(root_inode, filename);
    if (inum == -1) return -ENOENT;
    // Finds file then decrease file count
    inode_t* inode = get_inode(inum);
    inode->refs--;
    // Removes file from directory
    directory_delete(root_inode, filename);
    if (inode->refs <= 0) {
        free_inode(inum);
    } 
    return 0;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char *from, const char *to) {
    // Finds and deletes file
    inode_t* root_inode = get_inode(0);
    int inum = directory_lookup(root_inode, from + 1);
    // If file does not exist
    if (inum == -1) return -ENOENT;
    if (directory_lookup(root_inode, to + 1) != -1) {
        directory_delete(root_inode, to + 1);
    }
    // Adds file back with new name
    directory_delete(root_inode, from + 1);
    return directory_put(root_inode, to + 1, inum);
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int nufs_mkdir(const char *path, mode_t mode) {
    const char* dirname = (path[0] == '/') ? path + 1 : path;
    // If directory with same name exists
    if (find_inode_by_name(dirname) != -1) {
        return -EEXIST;
    }
    // Allocate and initialize inode for new directory
    int inum = alloc_inode();
    // If ran out of space
    if (inum == -1) {
        return -ENOSPC;
    }
    inode_t* inode = get_inode(inum);
    inode->refs = 1;
    inode->mode = S_IFDIR | mode;
    inode->size = 0;
    inode->blocks[0] = alloc_block();
    for (int i = 1; i < 4; i++) {
        inode->blocks[i] = -1;
    }
    // Adds directory
    inode_t* root_inode = get_inode(0);
    int rv = directory_put(root_inode, dirname, inum);
    if (rv < 0) {
        free_block(inode->blocks[0]);
        free_inode(inum);
        return rv;
    }
    return 0;
}

void nufs_init_ops(struct fuse_operations *ops) {
    memset(ops, 0, sizeof(struct fuse_operations));
    // Functions for operations 
    ops->getattr = nufs_getattr;
    ops->readdir = nufs_readdir;
    ops->mknod = nufs_mknod;
    ops->unlink = nufs_unlink;
    ops->rename = nufs_rename;
    ops->read = nufs_read;
    ops->write = nufs_write;
    ops->mkdir = nufs_mkdir;
}
struct fuse_operations nufs_ops;
int main(int argc, char *argv[]) {
    assert(argc > 2 && argc < 6);
    blocks_init(argv[--argc]);
    directory_init();
    nufs_init_ops(&nufs_ops);
    return fuse_main(argc, argv, &nufs_ops, NULL);
}
