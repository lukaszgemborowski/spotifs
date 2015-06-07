#ifndef SPOTIFS_SFS_H
#define SPOTIFS_SFS_H

#include <stdlib.h>

struct track;
struct playlist;

enum sfs_type {
    sfs_directory = 1 << 0,
    sfs_track = 1 << 1,
    sfs_playlist = 1 << 2,
    sfs_container = 1 << 3
};

struct sfs_entry {
    char* name;
    int type;
    size_t size;

    struct sfs_entry* next;
    struct sfs_entry* children;

    union {
        struct track* track;
        struct playlist* playlist;
    };
};

struct sfs_entry* sfs_get(struct sfs_entry* root, const char* path);
struct sfs_entry* sfs_add_child_entry(struct sfs_entry* root, struct sfs_entry* entry);
struct sfs_entry* sfs_add_child(struct sfs_entry* root, const char* name, int type);
struct sfs_entry* sfs_add_subdirectory(struct sfs_entry* root, const char* name);
struct sfs_entry* sfs_get_child_by_name(struct sfs_entry* root, const char* name);
struct sfs_entry* sfs_get_child_by_index(struct sfs_entry* root, int index);

#endif //SPOTIFS_SFS_H
