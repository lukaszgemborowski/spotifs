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

    logger_message(ctx, "get_track_from_library: %s; track: %s\n", playlist_name, track_name);
    for (; playlist != NULL; playlist = playlist->next)
    {
        if (strcmp(playlist_name, playlist->title) == 0)
        {
            logger_message(ctx, "get_track_from_library: playlist found\n");
            // playlist found
            struct track* track = playlist->tracks;

            for (; track != NULL; track = track->next)
            {
                // track found
                if (strcmp(track_name, track->title) == 0)
                {
                    logger_message(ctx, "get_track_from_library: track found\n");
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
                track->size = spotify_calculate_filezsize(ctx, track);
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
        // main layout of spotifs
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);
        filler(buf, s_library_path + 1, NULL, 0);
        filler(buf, s_artists_path + 1, NULL, 0);
    }
    else if(strcmp(path, s_library_path) == 0)
    {
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        const struct playlist* playlist = spotify_get_user_playlists(ctx);
        for (; playlist != NULL; playlist = playlist->next)
        {
            filler(buf, playlist->title, NULL, 0);
        }
    }
    else if (library_playlist_path(path))
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
                // fetch song list from playlist
                const struct track* track = playlist->tracks;

                for (; track != NULL; track = track->next)
                {
                    filler(buf, track->title, NULL, 0);
                }
            }
        }
    }
    else
    {
        return -ENOENT;
    }

    return 0;
}

// assemble list of callbacks
struct fuse_operations spotifs_operations =
{
    .getattr = spotifs_getattr,
    .readdir = spotifs_readdir,
/*.open = hello_open,
.read = hello_read,*/
};
