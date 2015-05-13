#include "sfs.h"
#include <string.h>
#include <malloc.h>

struct sfs_entry* sfs_get(struct sfs_entry* root, const char* path)
{
    if (!strcmp("/", path)) {
        return root;
    } else {
        char* copy = strdup(path);
        char* p = strtok(copy, "/");
        struct sfs_entry* entry = root->children;

        while (entry) {
            if (!strcmp(entry->name, p)) {
                /* component found */
                p = strtok(NULL, "/");

                if (!p) {
                    /* no more subdirs */
                    free(copy);
                    return entry;
                } else {
                    entry = entry->children;
                }
            } else {
                entry = entry->next;
            }
        }

        free(copy);
        return NULL;
    }
}

struct sfs_entry* sfs_add_child(struct sfs_entry* root, const char* name, int type)
{
    struct sfs_entry* entry = malloc(sizeof(struct sfs_entry));

    entry->next = NULL;
    entry->name = strdup(name);
    entry->size = 0;
    entry->type = type;
    entry->children = NULL;

    return sfs_add_child_entry(root, entry);
}

struct sfs_entry* sfs_add_child_entry(struct sfs_entry* root, struct sfs_entry* entry)
{
    entry->next = NULL;

    if (root->children) {
        struct sfs_entry *current = root->children;

        while (current->next) {
            current = current->next;
        }

        current->next = entry;
    } else {
        root->children = entry;
    }

    return entry;
}

struct sfs_entry* sfs_add_subdirectory(struct sfs_entry* root, const char* name)
{
    struct sfs_entry* entry = malloc(sizeof(struct sfs_entry));

    entry->next = NULL;
    entry->type = sfs_directory;
    entry->name = strdup(name);
    entry->size = 0;
    entry->children = NULL;

    return sfs_add_child_entry(root, entry);
}