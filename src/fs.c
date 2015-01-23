#include "fs.h"
#include "spotify.h"
#include "context.h"
#include "logger.h"
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>

static const char * const s_library_path = "/Library";
static const char * const s_artists_path = "/Artists";

// check if provided string is contained in layout array
// layout array is a structure of FS root directory
int string_in_layout(const char *str)
{
    if (strcmp(str, s_library_path) == 0 ||
        strcmp(str, s_artists_path) == 0)
        return 1;
    else
        return 0;
}

// check if provided path is path to playlist in user's "library"
// for example it shoul return non-zero value for /Library/My Songs/
// but zero for /Library or /Library/My Songs/Song.wav
int library_playlist_path(const char* path)
{
    char* bc = strdup(path);
    char* dc = strdup(path);

    char *b = basename(bc);
    char *d = dirname(dc);

    // starts with /Library/something
    if (strcmp(d, s_library_path))
    {
        return 0;
    }

    // for now, format is correct, but we should check
    // if playlist really exist, this is TODO

    free(bc);
    free(dc);

    return 1;
}

// check if provided path is path to song in user's playlist library,
// it should have form of /library/my playlist/some song
int track_in_library(const char* path)
{
    char* dc = strdup(path);

    // dirname of path should be proper playlist path
    int ret = library_playlist_path(dirname(dc));
    free (dc);

    return ret;
}

struct track* get_track_from_library(const char* path)
{
    struct spotifs_context* ctx = get_global_context;

    // path is in form of /library/playlist/song
    char *dc = strdup(path);
    char *bc = strdup(path);

    // now we have /library/playlist in library_path
    char *library_path = dirname(dc);
    char *track_name = basename(bc);

    // get only playlist name
    char *playlist_name = strchr(library_path + 1, '/') + 1;

    // iterate all playlists
    const struct playlist* playlist = spotify_get_user_playlists(ctx);

    //logger_message(ctx, "get_track_from_library: %s; track: %s\n", playlist_name, track_name);
    for (; playlist != NULL; playlist = playlist->next)
    {
        if (strcmp(playlist_name, playlist->title) == 0)
        {
            //logger_message(ctx, "get_track_from_library: playlist found\n");
            // playlist found
            struct track* track = playlist->tracks;

            for (; track != NULL; track = track->next)
            {
                // track found
                if (strcmp(track_name, track->title) == 0)
                {
                    //logger_message(ctx, "get_track_from_library: track found\n");
                    return track;
                }
            }
        }
    }

    return NULL;
}

static int spotifs_getattr(const char *path, struct stat *stbuf)
{
    struct spotifs_context* ctx = get_global_context;
    logger_message(ctx, "spotifs_getattr: %s\n", path);

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0)
    {
        // root directory
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
    }
    else if (string_in_layout(path))
    {
        // directory in layout, eg Library or Layouts
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 1;
    }
    else if (library_playlist_path(path)) 
    {
        // playlist in Library directory
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 1;
    }
    else if(track_in_library(path))
    {
        // track
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;

        struct track* track = get_track_from_library(path);

        if (track)
        {
            if (track->size <= 0)
            {
                // size estimation, 16bit sample * two channels * 44100 samples/s * duration (ms)
                track->size = (2 * 2 * 441 * (track->duration / 10)) * 0.9;
            }

            stbuf->st_size = track->size;
        }
        else
        {
            stbuf->st_size = 1;
        }
    }
    else
    {
        return -ENOENT;
    }

    return 0;
}

static int spotifs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    struct spotifs_context* ctx = get_global_context;
    logger_message(ctx, "spotifs_readdir: %s\n", path);

    if (strcmp(path, "/") == 0)
    {
        logger_message(ctx, "spotifs_readdir: root\n");
        // main layout of spotifs
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);
        filler(buf, s_library_path + 1, NULL, 0);
        filler(buf, s_artists_path + 1, NULL, 0);
    }
    else if(strcmp(path, s_library_path) == 0)
    {
        logger_message(ctx, "spotifs_readdir: library path\n");

        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        const struct playlist* playlist = spotify_get_user_playlists(ctx);
        for (; playlist != NULL; playlist = playlist->next)
        {
            logger_message(ctx, "spotifs_readdir: listing %s\n", playlist->title);
            filler(buf, playlist->title, NULL, 0);
        }
    }
    else if (library_playlist_path(path))
    {
        //logger_message(ctx, "spotifs_readdir: playlist path\n");
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
                //logger_message(ctx, "spotifs_readdir: match\n");

                // fetch song list from playlist
                const struct track* track = playlist->tracks;

                for (; track != NULL; track = track->next)
                {
                    //logger_message(ctx, "spotifs_readdir: track: %s\n", track->title);
                    filler(buf, track->title, NULL, 0);
                }

                break;
            }
        }
    }
    else
    {
        logger_message(ctx, "spotifs_readdir: ENOENT\n");
        return -ENOENT;
    }

    return 0;
}

int spotifs_open(const char *filename, struct fuse_file_info *info)
{
    struct spotifs_context* ctx = get_global_context;
    char* dirc = strdup(filename);
    char* path = dirname(dirc);

    logger_message(ctx, "%s: %s\n", __FUNCTION__, filename);

    if (library_playlist_path(path))
    {
        // get track
        struct track* track = get_track_from_library(filename);

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
            free(dirc);
            return -EACCES;
        }
    }
    else
    {
        free(dirc);
        return -EACCES;
    }

    free(dirc);
    return 0;
}

int spotifs_release(const char *filename, struct fuse_file_info *info)
{
    struct spotifs_context* ctx = get_global_context;
    char* dirc = strdup(filename);
    char* path = dirname(dirc);

    logger_message(ctx, "%s: %s\n", __FUNCTION__, filename);

    if (library_playlist_path(path))
    {
        // get track
        struct track* track = get_track_from_library(filename);
        track->refs --;

        if (0 == track->refs)
        {
            spotify_buffer_stop(ctx, track);
        }

        info->fh = 0;
    }
    else
    {
        free(dirc);
        return -EACCES;
    }

    free(dirc);
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
    // .filesize
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
    // .datasize
};

int spotifs_read(const char *filename, char *buffer, size_t size, off_t offset, struct fuse_file_info *info)
{
    (void) filename;

    struct spotifs_context* ctx = get_global_context;
    struct track* track = (struct track *)info->fh;

    logger_message(ctx, "%s: %s, size: %d, offset: %d\n", __FUNCTION__, filename, size, offset);

    if (offset < sizeof(struct wave_header))
    {
        // fill header informations
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
        size = track->size + sizeof(struct wave_header) - offset;
    }

    if (offset + size < sizeof(struct wave_header))
    {
        // read only within header range
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
