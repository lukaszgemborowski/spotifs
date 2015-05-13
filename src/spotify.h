#ifndef SPOTIFS_SPOTIFY_H
#define SPOTIFS_SPOTIFY_H

#include "context.h"
#include <stdint.h>
#include <pthread.h>

struct sfs_entry;

struct track
{
    char* title;
    char* filename;
    int duration;
    int channels;
    int sample_rate;
    int size;
    int refs;
    uint8_t* buffer;
    size_t buffer_pos;
    struct sp_track* spotify_track;
    struct track* next;
    pthread_mutex_t lock;
};

struct playlist
{
    char* title;
    struct track* tracks;
    struct sp_playlist* sp_playlist;
    struct playlist* next;
};

struct sfs_entry* spotify_get_root();
int spotify_connect(struct spotifs_context* ctx, const char *username, const char *password);
void spotify_disconnect(struct spotifs_context* ctx);
int spotify_buffer_track(struct spotifs_context* ctx, struct track* track);
void spotify_buffer_stop(struct spotifs_context* ctx, struct track* track);
void* spotify_buffer_read(struct spotifs_context* ctx, struct track* track, off_t offset, size_t size);

#endif // SPOTIFS_SPOTIFY_H
