#ifndef SPOTIFS_SPOTIFY_H
#define SPOTIFS_SPOTIFY_H

#include "context.h"
#include <stdint.h>
#include <pthread.h>
#include "buffer.h"

struct sfs_entry;

struct stream_buffer
{
    char* data;
    off_t size;
    off_t offset;
    size_t capacity;
};

struct track
{
    int duration;
    int channels;
    int sample_rate;
    int size;
    int refs;

    struct stream_buffer buffer;

    struct sp_track* spotify_track;
    pthread_mutex_t lock;
};

struct playlist
{
    struct sp_playlist* sp_playlist;
};

struct sfs_entry* spotify_get_root();
struct sfs_entry* spotify_get_playlists();

int spotify_connect(struct spotifs_context* ctx, const char *username, const char *password);
void spotify_disconnect(struct spotifs_context* ctx);

int spotify_buffer_track(struct spotifs_context* ctx, struct track* track);
void spotify_buffer_stop(struct spotifs_context* ctx, struct track* track);
int spotify_read(struct spotifs_context* ctx, struct track* track, off_t offset, size_t size, char *buffer);
struct track* spotify_current(struct spotifs_context* ctx);

#endif // SPOTIFS_SPOTIFY_H
