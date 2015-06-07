#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include <glib.h>
#include "spotify.h"
#include "context.h"
#include "logger.h"
#include "fs.h"
#include "sfs.h"

#define get_app_context fuse_get_context()->private_data;

static int fuse_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));

    struct sfs_entry* entry = sfs_get(spotify_get_root(), path);

    if (entry) {
        if (entry->type & sfs_track) {
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
        } else if (entry->type & sfs_directory) {
            stbuf->st_mode = S_IFDIR | 0555;
            stbuf->st_nlink = 2;
        }

        stbuf->st_size = entry->size;

        return 0;
    } else {
        return -ENOENT;
    }
}

static int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    struct sfs_entry* dir;
    g_debug("%s: %s", __func__, path);

    dir = sfs_get(spotify_get_root(), path);

    g_debug("%s: dir name %s", __func__, dir->name);

    if (dir && dir->type & sfs_directory) {

        struct sfs_entry* item = dir->children;

        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        while (item) {
            g_debug("%s: name %s", __func__, item->name);
            filler(buf, item->name, NULL, 0);
            item = item->next;
        }

        return 0;
    } else {
        return -ENOENT;
    }
}

int fuse_open(const char *filename, struct fuse_file_info *info)
{
    struct spotifs_context* ctx = get_app_context;
    struct sfs_entry* track = sfs_get(spotify_get_root(), filename);
    g_debug("%s: %s", __func__, filename);

    if (track && (track->type & sfs_track)) {
        if (!track->track->refs) {
            if (spotify_buffer_track(ctx, track->track) < 0) {
                return -EIO;
            }
        }

        track->track->refs ++;
        info->fh = (uint64_t)track->track;
        return 0;
    } else {
        return -ENOENT;
    }
}

int fuse_release(const char *filename, struct fuse_file_info *info)
{
    struct spotifs_context* ctx = get_app_context;
    struct sfs_entry* track = sfs_get(spotify_get_root(), filename);
    g_debug("%s: %s", __func__, filename);

    if (track && (track->type & sfs_track)) {
        track->track->refs --;

        if (!track->track->refs) {
            spotify_buffer_stop(ctx, track->track);
        }

        return 0;
    } else {
        return -ENOENT;
    }
}

int fuse_read(const char *filename, char *buffer, size_t size, off_t offset, struct fuse_file_info *info)
{
    (void) filename;

    struct spotifs_context* ctx = get_app_context;
    struct track* track = (struct track *)info->fh;

    g_debug("%s: %s, size: %zu, offset: %zu", __func__, filename, size, offset);

    return spotify_read(ctx, track, offset, size, buffer);
}

// assemble list of callbacks
struct fuse_operations spotifs_operations =
{
    .getattr = fuse_getattr,
    .readdir = fuse_readdir,
    .open = fuse_open,
    .release = fuse_release,
    .read = fuse_read,
};
