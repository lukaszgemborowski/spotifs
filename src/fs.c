#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>

#include "path_operations.h"
#include "spotify.h"
#include "context.h"
#include "logger.h"
#include "fs.h"

#define get_app_context fuse_get_context()->private_data;

static int spotifs_getattr(const char *path, struct stat *stbuf)
{
    struct spotifs_context* ctx = get_app_context;
    logger_message(ctx, "spotifs_getattr: %s\n", path);

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0 || is_path_in_root(path) || is_library_playlist_path(path))
    {
        // one of directories in our filesystem,
        // set standard mode for directory
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
    }
    else if(is_path_to_library_track(path))
    {
        // track which is regular file
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;

        // get reference to track from spotify
        struct track* track = get_track_from_library(ctx, path);

        if (track)
        {
            if (track->size <= 0)
            {
                // size estimation, 16bit sample * two channels * 44100 samples/s * duration (ms)
                // I don't know if we can do any better at this point, to get proper track size
                // we would need to start playback to get sample rate, channels count etc.
                track->size = (2 * 2 * 441 * (track->duration / 10)) * 0.9;
            }

            stbuf->st_size = track->size + 44; // ugly 44 bytes, WAV header size, need to be refactored
        }
        else
        {
            // can't find track in spotify, return error
            stbuf->st_size = -1;
            return -ENOENT;
        }
    }
    else
    {
        // not handled, error
        return -ENOENT;
    }

    return 0;
}

static int spotifs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    struct spotifs_context* ctx = get_app_context;
    logger_message(ctx, "spotifs_readdir: %s\n", path);

    if (strcmp(path, "/") == 0)
    {
        // main layout of spotifs
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        const char **directories = get_root_layout();

        while (*directories)
        {
            filler(buf, *directories, NULL, 0);
            directories ++;
        }
    }
    else if(is_library_path(path))
    {
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        const struct playlist* playlist = spotify_get_user_playlists(ctx);
        for (; playlist != NULL; playlist = playlist->next)
        {
            filler(buf, playlist->title, NULL, 0);
        }
    }
    else if (is_library_playlist_path(path))
    {
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        // this is playlist directory, eg. "/Library/My Playlist 3"
        // playlist name is string after last slash '/'.
        const char* playlist_name = strrchr(path, '/') + 1;

        const struct playlist* playlist = spotify_get_user_playlists(ctx);

        // find playlist in user's playlists
        for (; playlist != NULL; playlist = playlist->next)
        {
            if (strcmp(playlist->title, playlist_name) == 0)
            {
                // fetch song list from playlist and fill directory entry
                const struct track* track = playlist->tracks;

                for (; track != NULL; track = track->next)
                {
                    filler(buf, track->title, NULL, 0);
                }

                break;
            }
        }
    }
    else
    {
        return -ENOENT;
    }

    return 0;
}

int spotifs_open(const char *filename, struct fuse_file_info *info)
{
    struct spotifs_context* ctx = get_app_context;
    char* dirc = strdup(filename);
    char* path = dirname(dirc);
    int ret = 0;

    logger_message(ctx, "%s: %s\n", __FUNCTION__, filename);

    if (is_library_playlist_path(path))
    {
        // get track from playlist
        struct track* track = get_track_from_library(ctx, filename);

        if (track)
        {
            track->refs ++;

            if (track->refs == 1)
            {
                spotify_buffer_track(ctx, track);
            }

            info->fh = (size_t)track;
            info->nonseekable = 1;
        }
        else
        {
            ret = -EACCES;
        }
    }
    else
    {
        ret = -EACCES;
    }

    free(dirc);
    return ret;
}

int spotifs_release(const char *filename, struct fuse_file_info *info)
{
    struct spotifs_context* ctx = get_app_context;
    char* dirc = strdup(filename);
    char* path = dirname(dirc);
    int ret = 0;

    logger_message(ctx, "%s: %s\n", __FUNCTION__, filename);

    if (is_library_playlist_path(path))
    {
        // get track
        struct track* track = get_track_from_library(ctx, filename);
        track->refs --;

        if (0 == track->refs)
        {
            // stop buffering if there is no more refs
            spotify_buffer_stop(ctx, track);
        }

        info->fh = 0;
    }
    else
    {
        ret = -EACCES;
    }

    free(dirc);
    return ret;
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
