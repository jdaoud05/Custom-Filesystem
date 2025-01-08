#include "directory.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "slist.h"

// Initialize the root directory inode
void directory_init() {
    inode_t* root_inode = get_inode(0); // Get the root inode (inode 0)
    root_inode->refs = 1;              // Set reference count to 1
    root_inode->mode = 040755;         // Set mode to directory with default permissions
    root_inode->size = 0;              // Initialize size to 0
    root_inode->blocks[0] = alloc_block(); // Allocate the first block for the directory
    for (int i = 1; i < 4; i++) {      // Set remaining blocks to -1 (unallocated)
        root_inode->blocks[i] = -1;
    }
}

// Lookup a directory entry by name
int directory_lookup(inode_t *di, const char *name) {
    if (di->blocks[0] == -1) return -1; // If no block is allocated, return -1
    dir_entry_t* entries = (dir_entry_t*)blocks_get_block(di->blocks[0]); // Get the first block
    int count = di->size / sizeof(dir_entry_t); // Calculate the number of entries
    // Iterate through directory entries
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, name) == 0) { // Match found
            return entries[i].inum; // Return inode number
        }
    }
    return -1; // Entry not found
}

// Add a new entry to the directory
int directory_put(inode_t *di, const char *name, int inum) {
    if (strlen(name) >= DIR_NAME_LENGTH) return -ENAMETOOLONG; // Check for name length
    if (di->blocks[0] == -1) { // If no block is allocated, allocate one
        di->blocks[0] = alloc_block();
        if (di->blocks[0] == -1) return -ENOSPC; // No space available
        di->size = 0;
    }

    dir_entry_t* entries = (dir_entry_t*)blocks_get_block(di->blocks[0]); // Get the first block
    int count = di->size / sizeof(dir_entry_t); // Calculate the number of entries
    // Check for duplicate entry
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, name) == 0) {
            return -EEXIST; // Entry already exists
        }
    }

    // Check if there is enough space for the new entry
    if ((count + 1) * sizeof(dir_entry_t) > BLOCK_SIZE) return -ENOSPC;
    // Add the new entry
    strncpy(entries[count].name, name, DIR_NAME_LENGTH - 1); // Copy name
    entries[count].name[DIR_NAME_LENGTH - 1] = '\0'; // Ensure null termination
    entries[count].inum = inum; // Assign inode number
    di->size += sizeof(dir_entry_t); // Update directory size
    return 0;
}

// Remove a directory entry by name
int directory_delete(inode_t *di, const char *name) {
    if (di->blocks[0] == -1) return -ENOENT; // No block allocated, entry not found
    dir_entry_t* entries = (dir_entry_t*)blocks_get_block(di->blocks[0]); // Get the first block
    int count = di->size / sizeof(dir_entry_t); // Calculate the number of entries
    // Find and delete the entry
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, name) == 0) { // Match found
            if (i < count - 1) { // If not the last entry, shift entries
                memmove(&entries[i], &entries[i + 1], 
                       (count - i - 1) * sizeof(dir_entry_t));
            }
            di->size -= sizeof(dir_entry_t); // Update directory size
            return 0; // Deletion successful
        }
    }
    return -ENOENT; // entry not found
}

// List all entries in the directory
slist_t *directory_list(inode_t *di) {
    if (di->blocks[0] == -1) return 0; // No block allocated, return null
    dir_entry_t* entries = (dir_entry_t*)blocks_get_block(di->blocks[0]); // Get the first block
    int count = di->size / sizeof(dir_entry_t); // Calculate the number of entries
    slist_t* list = 0; // Initialize the list
    // Add each entry to the list
    for (int i = 0; i < count; i++) {
        list = s_cons(strdup(entries[i].name), list); // Add entry name to the list
    }
    return list; // Return the list
}
