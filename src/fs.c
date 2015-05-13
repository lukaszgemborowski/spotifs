#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include "spotify.h"
#include "context.h"
#include "logger.h"
#include "fs.h"
#include "sfs.h"

#define get_app_context fuse_get_context()->private_data;

static int spotifs_getattr(const char *path, struct stat *stbuf)
{
    struct spotifs_context* ctx = get_app_context;
    logger_message(ctx, "%s: %s\n", __func__, path);

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

static int spotifs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    struct sfs_entry* dir;
    struct spotifs_context* ctx = get_app_context;
    logger_message(ctx, "%s: %s\n", __func__, path);

    dir = sfs_get(spotify_get_root(), path);

    if (dir && dir->type & sfs_directory) {
        struct sfs_entry* item = dir->children;

        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        while (item) {
            filler(buf, item->name, NULL, 0);
            item = item->next;
        }
    } else {
        return -ENOENT;
    }
}

int spotifs_open(const char *filename, struct fuse_file_info *info)
{
    struct spotifs_context* ctx = get_app_context;
    logger_message(ctx, "%s: %s\n", __FUNCTION__, filename);
    return -EACCES;
}

int spotifs_release(const char *filename, struct fuse_file_info *info)
{
    struct spotifs_context* ctx = get_app_context;
    logger_message(ctx, "%s: %s\n", __FUNCTION__, filename);
    return 0;
}

struct wave_header
{
    char mark[4];
    int32_t overall_size;
    char wave[4];
    char fmt[4];
    int32_t format_len;
    int16_t format;
    int16_t channels;
    int32_t samplerate;
    int32_t samplerate2;
    int16_t channelrate;
    int16_t bitspersample;
    char data[4];
    int32_t datasize;
} __attribute__((packed));

static struct wave_header header = {
    .mark = {'R', 'I', 'F', 'F'},
    // .filesize - set in spotifs_read
    .wave = {'W', 'A', 'V', 'E'},
    .fmt = {'f', 'm', 't', ' '},
    .format_len = 16,
    .format = 1,
    .channels = 2,
    .samplerate = 44100,
    .samplerate2 = 176400,
    .channelrate = 4,
    .bitspersample = 16,
    .data = {'d', 'a', 't', 'a'}
    // .datasize - set in spotifs_read
};

int spotifs_read(const char *filename, char *buffer, size_t size, off_t offset, struct fuse_file_info *info)
{
    #if 0
    (void) filename;

    struct spotifs_context* ctx = get_app_context;
    struct track* track = (struct track *)info->fh;

    logger_message(ctx, "%s: %s, size: %d, offset: %d\n", __FUNCTION__, filename, size, offset);

    // read offset is within WAVE header
    // in this case header should be ready
    if (offset < sizeof(struct wave_header))
    {
        // be sure that track is already buffered,
        // this will ensure that size calculation is correct
        spotify_buffer_read(ctx, track, 0, 128);

        header.overall_size = sizeof(struct wave_header) + track->size - 8;
        header.datasize = track->size;
    }

    if (offset >= track->size + sizeof(struct wave_header))
    {
        // can't read beyond track size
        return 0;
    }

    if (offset + size >= track->size + sizeof(struct wave_header))
    {
        // client is trying to read more than overall file size,
        // correct size variable.
        size = track->size + sizeof(struct wave_header) - offset;
    }

    if (offset + size < sizeof(struct wave_header))
    {
        // read only within header range, just memcpy header
        memcpy(buffer, ((char *)&header) + offset, size);
    }
    else if (offset < sizeof(struct wave_header))
    {
        // read part of header and part of track data

        // read part of header
        const size_t header_bytes_to_read = sizeof(struct wave_header) - offset;

        memcpy(buffer, ((char *)&header) + offset, header_bytes_to_read);

        // read track data
        void *data = spotify_buffer_read(ctx, track, 0, size - header_bytes_to_read);

        // copy data to buffer
        memcpy(buffer + header_bytes_to_read, data, size - header_bytes_to_read);
    }
    else
    {
        // read request after header
        void *data = spotify_buffer_read(ctx, track, offset - sizeof(struct wave_header), size);
        memcpy(buffer, data, size);
    }

    return size;
    #endif

    return -EACCES;
}

// assemble list of callbacks
struct fuse_operations spotifs_operations =
{
    .getattr = spotifs_getattr,
    .readdir = spotifs_readdir,
    .open = spotifs_open,
    .release = spotifs_release,
    .read = spotifs_read
};
