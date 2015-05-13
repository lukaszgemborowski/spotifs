#include "spotify.h"
#include "spotify_appkey.h"
#include "logger.h"

#include <pthread.h>
#include <string.h>
#include <assert.h>
#include "support.h"
#include "sfs.h"

static struct track* g_current_track = NULL;
static pthread_mutex_t current_track_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t current_track_cond = PTHREAD_COND_INITIALIZER;

/* global playlist lock */
struct sfs_entry_list {
    struct sfs_entry first;
    pthread_mutex_t lock;
};

static struct sfs_entry_list g_directory = {
        .first = {
                .type = sfs_directory,
                .name = "/",
                .size = 0,
                .children = NULL
        },
        .lock = PTHREAD_MUTEX_INITIALIZER
};

/* worker thread variables */
static pthread_t spotify_worker_thread_handle;

struct sfs_entry* spotify_get_root() { return &g_directory.first; }

static void* spotify_worker_thread(void *param)
{
    struct spotifs_context* ctx = param;

    int next_timeout = 0;
    sp_error err;

    while (ctx->worker_running)
    {
        pthread_mutex_lock(&ctx->lock);

        while (!ctx->spotify_event)
            pthread_cond_wait(&ctx->change, &ctx->lock);

        ctx->spotify_event = 0;

        pthread_mutex_unlock(&ctx->lock);

        err = sp_session_process_events(ctx->spotify_session, &next_timeout);

        if (SP_ERROR_OK != err)
        {
            logger_message(ctx, "spotify_worker_thread: error: %s.\n", sp_error_message(err));
        }
    }
}

void sp_cb_playlist_metadata_updated(sp_playlist *pl, void *userdata)
{
    /* ignore */
    (void *)pl;
    (void *)userdata;
}

static sp_playlist_callbacks pl_callbacks = {
    .playlist_metadata_updated = &sp_cb_playlist_metadata_updated
};


static void initialize_playlists(struct spotifs_context* ctx, sp_playlistcontainer *container)
{
    const int num_playlists = sp_playlistcontainer_num_playlists(container);
    int i, j;
    (void *)ctx;

    struct sfs_entry *library = sfs_get(&g_directory.first, "/library");

    if (!num_playlists || !library) {
        return;
    }

    for (i = 0; i < num_playlists; i++) {
        struct sfs_entry *entry, *track_entry;
        struct playlist *playlist = malloc(sizeof(struct playlist));
        char *name;
        int songs_count;
        playlist->sp_playlist = sp_playlistcontainer_playlist (container, i);
        sp_playlist_add_callbacks(playlist->sp_playlist, &pl_callbacks, playlist);

        name = replace_character(strdup(sp_playlist_name(playlist->sp_playlist)), '/', '_');

        entry = sfs_add_child(library, name, sfs_directory | sfs_playlist);
        entry->playlist = playlist;

        free(name);

        /* create song list for playlist */
        songs_count = sp_playlist_num_tracks(playlist->sp_playlist);

        for (j = 0; j < songs_count; j++) {
            struct track* track = malloc(sizeof(struct track));
            track->spotify_track = sp_playlist_track(playlist->sp_playlist, j);

            name = malloc(strlen(sp_track_name(track->spotify_track)) + 5);
            strcpy(name, sp_track_name(track->spotify_track));
            strcat(name, ".wav");
            replace_character(name, '/', '_');

            track_entry = sfs_add_child(entry, name, sfs_track);
            track_entry->track = track;

            free(name);
        }
    }
}

static void sp_cb_container_loaded(sp_playlistcontainer *container, void *userdata)
{
    struct spotifs_context* ctx = userdata;

    logger_message(ctx, "%s\n", __func__);

    /* container was loaded, refresh playlists */
    pthread_mutex_lock(&g_directory.lock);
    initialize_playlists(ctx, container);
    pthread_mutex_unlock(&g_directory.lock);
}

static sp_playlistcontainer_callbacks pc_callbacks = {
    .container_loaded = &sp_cb_container_loaded,
};

// handle logged in event
static void sp_cb_logged_in(sp_session *sess, sp_error error)
{
    struct spotifs_context* ctx = sp_session_userdata(sess);

    assert(ctx != NULL);
    assert(ctx->spotify_session == sess);

    pthread_mutex_lock(&ctx->lock);

    if (SP_ERROR_OK != error)
    {
        ctx->logged_in = -1;
        logger_message(ctx, "sp_cb_logged_in: got error: %s\n", sp_error_message((error)));
    }
    else
    {
        ctx->logged_in = 1;
        logger_message(ctx, "sp_cb_logged_in: Logged in\n");
    }

    // signal that login is completed
    pthread_cond_signal(&ctx->change);
    pthread_mutex_unlock(&ctx->lock);

    logger_message(ctx, "%s: exit\n", __func__);
}

static void sp_cb_logged_out(sp_session *sess)
{
    struct spotifs_context* ctx = sp_session_userdata(sess);
    logger_message(ctx, "sp_cb_logged_out: Logged out.\n");

    pthread_mutex_lock(&ctx->lock);

    /* notify worker thread to exit */
    ctx->spotify_event = 1;
    ctx->worker_running = 0;

    pthread_cond_signal(&ctx->change);
    pthread_mutex_unlock(&ctx->lock);
}

static void sp_cb_notify_main_thread(sp_session *session)
{
    struct spotifs_context* ctx = sp_session_userdata(session);

    pthread_mutex_lock(&ctx->lock);
    ctx->spotify_event = 1;
    pthread_cond_signal(&ctx->change);
    pthread_mutex_unlock(&ctx->lock);
}

static int sp_cb_music_delivery(sp_session *session, const sp_audioformat *format, const void *frames, int num_frames)
{
    (void) session;

    if (!g_current_track)
        return num_frames;

    struct spotifs_context *ctx = sp_session_userdata(session);

    if (NULL == g_current_track->buffer)
    {
        logger_message(ctx, "sp_cb_music_delivery: allocating buffer: channels: %d, sample rate: %d\n", format->channels, format->sample_rate);

        // 2 * 2 * 441 * (track->duration / 10);
        // size is: 2 bytes (16 bit sample)  * channels * sample rate [sample/s] * duration [s] / 1000

        g_current_track->size = 2 * format->channels * (format->sample_rate / 100) * (g_current_track->duration / 10);
        g_current_track->sample_rate = format->sample_rate;
        g_current_track->channels = format->channels;
        g_current_track->buffer = malloc(g_current_track->size);
        g_current_track->buffer_pos = 0;
    }

    // assume that these values can't change
    assert(g_current_track->sample_rate == format->sample_rate);
    assert(g_current_track->channels == format->channels);

    const size_t data_bytes = num_frames * 2 * format->channels;
    memcpy(&g_current_track->buffer[g_current_track->buffer_pos], frames, data_bytes);
    g_current_track->buffer_pos += data_bytes;

    pthread_cond_signal(&current_track_cond);

    return num_frames;
}

static void sp_cb_connection_error(sp_session *session, sp_error error)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);;

    logger_message(ctx, "%s: %s\n", __FUNCTION__, sp_error_message(error));
}

static void sp_cb_play_token_lost(sp_session *session)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);

    logger_message(ctx, "Play token lost\n");
}

static void sp_cb_log_message(sp_session *session, const char *data)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);

    //logger_message(ctx, "%s: %s\n", __FUNCTION__, data);
}

static void sp_cb_end_of_track(sp_session *session)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);

    logger_message(ctx, "End of track\n");
}

static void streaming_error(sp_session *session, sp_error error)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);

    logger_message(ctx, "%s: %s\n", __FUNCTION__, sp_error_message(error));
}

static sp_session_callbacks session_callbacks = {
    .logged_in = &sp_cb_logged_in,
    .logged_out = &sp_cb_logged_out,
    .notify_main_thread = &sp_cb_notify_main_thread,
    .music_delivery = &sp_cb_music_delivery,
    .connection_error = &sp_cb_connection_error,
    .play_token_lost = &sp_cb_play_token_lost,
    .log_message = &sp_cb_log_message,
    .end_of_track = &sp_cb_end_of_track,
    .streaming_error = &streaming_error,
    NULL,
};

static sp_session_config spconfig = {
    .api_version = SPOTIFY_API_VERSION,
    .cache_location = "tmp",
    .settings_location = "tmp",
    .application_key = g_appkey,
    .application_key_size = 0,
    .user_agent = "spotify-fs-example",
    .callbacks = &session_callbacks,
    NULL,
};

int start_worker_thread(struct spotifs_context *ctx)
{
    assert(ctx->worker_running == 0);

    ctx->worker_running = 1;
    ctx->spotify_event = 1;

    int ret = pthread_create(&spotify_worker_thread_handle, NULL, spotify_worker_thread, ctx);

    if (ret)
    {
        ctx->worker_running = 0;
        ctx->spotify_event = 0;
    }

    return ret;
}

void stop_worker_thread(struct spotifs_context *ctx)
{
    assert(ctx->worker_running == 1);
    pthread_mutex_lock(&ctx->lock);

    ctx->worker_running = 0;
    ctx->spotify_event = 1;

    pthread_cond_signal(&ctx->change);
    pthread_mutex_unlock(&ctx->lock);

    pthread_join(spotify_worker_thread_handle, NULL);
}

int spotify_connect(struct spotifs_context* ctx, const char *username, const char *password)
{
    if (ctx->spotify_session || ctx->logged_in)
    {
        logger_message(ctx, "%s: session already created. Exiting.\n", __func__);
        return -1;
    }

    sfs_add_child(&g_directory.first, "library", sfs_directory | sfs_container);

    spconfig.application_key_size = g_appkey_size;
    spconfig.userdata = ctx;

    sp_error err = sp_session_create(&spconfig, &ctx->spotify_session);

    if (SP_ERROR_OK != err)
    {
        logger_message(ctx, "%s: sp_session_create failed: %s\n", __func__, sp_error_message(err));
        return -2;
    }

    /* we need to start worker thread at this point */
    if (0 != start_worker_thread(ctx))
    {
        logger_message(ctx, "%s: can't create worker thread\n", __func__);
        return -3;
    }

    /* login to spotify, this is asynchronous so we need to synchronise this call
       via mutex and conditional variable */
    sp_session_login(ctx->spotify_session, username, password, 0, NULL);

    pthread_mutex_lock(&ctx->lock);

    ctx->logged_in = 2;

    while (2 == ctx->logged_in) {
        pthread_cond_wait(&ctx->change, &ctx->lock);
    }

    ctx->spotify_playlist_container = sp_session_playlistcontainer(ctx->spotify_session);
    sp_playlistcontainer_add_callbacks(ctx->spotify_playlist_container, &pc_callbacks, ctx);

    pthread_mutex_unlock(&ctx->lock);

    return ctx->logged_in;
}

void spotify_disconnect(struct spotifs_context* ctx)
{
    if (!ctx->spotify_session || !ctx->logged_in)
    {
        logger_message(ctx, "spotify_connect: session is not created.\n");
        return;
    }

    stop_worker_thread(ctx);
}

int spotify_buffer_track(struct spotifs_context* ctx, struct track* track)
{
    logger_message(ctx, "%s\n", __func__);

    sp_error err;
    int ret = 0;

    pthread_mutex_lock(&current_track_mutex);
    assert(g_current_track == NULL);
    g_current_track = track;

    // load and play
    if(SP_ERROR_OK != (err = sp_session_player_load(ctx->spotify_session, track->spotify_track)))
    {
        logger_message(ctx, "spotify_buffer_track: sp_session_player_load: %s\n", sp_error_message(err));
        g_current_track = NULL;
        ret = -1;
    }

    if (SP_ERROR_OK != (err = sp_session_player_play(ctx->spotify_session, 1)))
    {
        logger_message(ctx, "spotify_buffer_track: sp_session_player_play: %s\n", sp_error_message(err));
        g_current_track = NULL;
        ret = -1;
    }

    pthread_mutex_unlock(&current_track_mutex);

    return ret;
}

void spotify_buffer_stop(struct spotifs_context* ctx, struct track* track)
{
    (void) track;

    logger_message(ctx, "%s\n", __FUNCTION__);
    pthread_mutex_lock(&current_track_mutex);

    assert(g_current_track != NULL);

    sp_session_player_play(ctx->spotify_session, 0);
    sp_session_player_unload(ctx->spotify_session);

    free(g_current_track->buffer);
    g_current_track->buffer_pos = 0;
    g_current_track->buffer = NULL;
    g_current_track = NULL;

    pthread_mutex_unlock(&current_track_mutex);
}

void* spotify_buffer_read(struct spotifs_context* ctx, struct track* track, off_t offset, size_t size)
{
    pthread_mutex_lock(&current_track_mutex);

    // wait for buffer
    while (size + offset > track->buffer_pos)
    {
        logger_message(ctx, "%s: read(%d, %d), buffer_pos: %d\n", __FUNCTION__, offset, size, track->buffer_pos);
        pthread_cond_wait(&current_track_cond, &current_track_mutex);
    }

    pthread_mutex_unlock(&current_track_mutex);

    // return pointer to buffer + offset
    return track->buffer + offset;
}
