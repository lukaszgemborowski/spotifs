#ifndef SPOTIFS_SPOTIFY_H
#define SPOTIFS_SPOTIFY_H

#include "context.h"

struct track
{
    char* title;
    int duration;
    int size;
    struct sp_track* spotify_track;
    struct track* next;
};

struct playlist
{
    char* title;
    struct track* tracks;
    struct playlist* next;
};

// this operation is synchronous so it may take a while to complete
int spotify_connect(struct spotifs_context* ctx, const char *username, const char *password);
void spotify_disconnect(struct spotifs_context* ctx);
const struct playlist* spotify_get_user_playlists(struct spotifs_context* ctx);
int spotify_calculate_filezsize(struct spotifs_context* ctx, struct track* track);

#endif // SPOTIFS_SPOTIFY_H
