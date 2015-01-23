#ifndef SPOTIFS_SPOTIFY_H
#define SPOTIFS_SPOTIFY_H

#include "context.h"
#include <stdint.h>

struct track
{
    char* title;
    int duration;
    int size;
    bool refs;
    uint8_t* buffer;
    size_t buffer_pos;
    struct sp_track* spotify_track;
    struct track* next;
};

struct playlist
{
    char* title;
    struct track* tracks;
    struct sp_playlist* spotify_playlist;
    struct playlist* next;
};

// this operation is synchronous so it may take a while to complete
int spotify_connect(struct spotifs_context* ctx, const char *username, const char *password);
void spotify_disconnect(struct spotifs_context* ctx);
const struct playlist* spotify_get_user_playlists(struct spotifs_context* ctx);
int spotify_buffer_track(struct spotifs_context* ctx, struct track* track);
void spotify_buffer_stop(struct spotifs_context* ctx, struct track* track);
void* spotify_buffer_read(struct spotifs_context* ctx, struct track* track, off_t offset, size_t size);

#endif // SPOTIFS_SPOTIFY_H
