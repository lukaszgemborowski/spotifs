#include "path_operations.h"
#include <string.h>
#include <libgen.h>
#include <stdlib.h>

#define D_LIBRARY_PATH "Library"
#define D_ARTISTS_PATH "Artists"

// directory names
static const char * const s_library_path = "/" D_LIBRARY_PATH;
static const char * const s_artists_path = "/" D_ARTISTS_PATH;

// array with all directories within root, NULL-terminated
static const char*  g_layout[] = {
    D_LIBRARY_PATH,
    D_ARTISTS_PATH,
    NULL
};

const char** get_root_layout()
{
    // return root layout
    return g_layout;
}

bool is_library_path(const char *directory)
{
    return strcmp(directory, s_library_path) == 0;
}

bool is_path_in_root(const char *str)
{
    // check if provided string is contained in layout array
    // layout array is a structure of FS root directory
    if (strcmp(str, s_library_path) == 0 ||
        strcmp(str, s_artists_path) == 0)
        return 1;
    else
        return 0;
}

bool is_library_playlist_path(const char* path)
{
    // check if provided path is path to playlist in user's "library"
    // for example it shoul return non-zero value for /Library/My Songs/
    // but zero for /Library or /Library/My Songs/Song.wav
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

bool is_path_to_library_track(const char* path)
{
    // check if provided path is path to song in user's playlist library,
    // it should have form of /library/my playlist/some song
    char* dc = strdup(path);

    // dirname of path should be proper playlist path
    int ret = is_library_playlist_path(dirname(dc));
    free (dc);

    return ret;
}

struct track* get_track_from_library(const char* path)
{
    struct spotifs_context* ctx = get_global_context;

    // path is in form of /library/playlist/song
    // FIXME: memory leak detected!
    char *dc = strdup(path);
    char *bc = strdup(path);

    // now we have /library/playlist in library_path
    char *library_path = dirname(dc);
    char *track_name = basename(bc);

    // get only playlist name
    char *playlist_name = strchr(library_path + 1, '/') + 1;

    // iterate all playlists
    const struct playlist* playlist = spotify_get_user_playlists(ctx);

    for (; playlist != NULL; playlist = playlist->next)
    {
        if (strcmp(playlist_name, playlist->title) == 0)
        {
            // playlist found
            struct track* track = playlist->tracks;

            for (; track != NULL; track = track->next)
            {
                // track found
                if (strcmp(track_name, track->title) == 0)
                {
                    return track;
                }
            }
        }
    }

    return NULL;
}
