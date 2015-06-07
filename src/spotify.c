#include "spotify.h"
#include "spotify_appkey.h"
#include "logger.h"

#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <math.h>
#include <glib.h>
#include "support.h"
#include "sfs.h"
#include "wave.h"

static struct track* g_current_track = NULL;
static pthread_mutex_t current_track_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutexattr_t current_track_mutex_attr;
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
struct sfs_entry* spotify_get_playlists()
{
    struct sfs_entry* entry = spotify_get_root()->children;

    while (entry) {
        if (!strcmp(entry->name, "library")) {
            return entry;
        }

        entry = entry->next;
    }

    return NULL;
}

static void* spotify_worker_thread(void *param)
{
    struct spotifs_context* ctx = param;
    struct timespec timeout;

    int next_timeout = 0;
    sp_error err;

    ctx->spotify_event = 1;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 1;

    while (ctx->worker_running)
    {
        pthread_mutex_lock(&ctx->lock);

        while (!ctx->spotify_event) {
            /*if (timeout.tv_nsec < 0 || timeout.tv_nsec > 1000000000L) {
                timeout.tv_nsec = 0;
                timeout.tv_sec ++;
            }*/

            timeout.tv_sec ++;

            if (pthread_cond_timedwait(&ctx->change, &ctx->lock, &timeout) == ETIMEDOUT) {
                g_debug("%s: timedout", __func__);
                break;
            }
        }

        ctx->spotify_event = 0;

        pthread_mutex_unlock(&ctx->lock);

        do {
            err = sp_session_process_events(ctx->spotify_session, &next_timeout);
        } while(next_timeout == 0 && err == SP_ERROR_OK);

        clock_gettime(CLOCK_REALTIME, &timeout);

        //timeout.tv_sec += next_timeout / 1000;
        //timeout.tv_nsec += (next_timeout % 1000) * 1000000L;

        timeout.tv_sec ++;

        if (SP_ERROR_OK != err) {
            g_error("%s: error: '%s'", __func__, sp_error_message(err));
        }
    }
}

void sp_cb_playlist_metadata_updated(sp_playlist *pl, void *userdata)
{
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
        int num_songs;
        playlist->sp_playlist = sp_playlistcontainer_playlist (container, i);
        sp_playlist_add_callbacks(playlist->sp_playlist, &pl_callbacks, playlist);

        name = replace_character(strdup(sp_playlist_name(playlist->sp_playlist)), '/', '_');

        entry = sfs_add_child(library, name, sfs_directory | sfs_playlist);
        entry->playlist = playlist;

        free(name);

        /* create song list for playlist */
        num_songs = sp_playlist_num_tracks(playlist->sp_playlist);

        for (j = 0; j < num_songs; j++) {
            struct track* track = malloc(sizeof(struct track));
            memset(track, 0, sizeof(struct track));
            track->spotify_track = sp_playlist_track(playlist->sp_playlist, j);
            track->duration = sp_track_duration(track->spotify_track);

            name = malloc(strlen(sp_track_name(track->spotify_track)) + 5);
            strcpy(name, sp_track_name(track->spotify_track));
            strcat(name, ".wav");
            replace_character(name, '/', '_');

            track_entry = sfs_add_child(entry, name, sfs_track);
            track_entry->track = track;

            track_entry->size = wave_size(2, 2, 44100, (ceil(track->duration/1000.) + 1)) + wave_header_size();

            free(name);
        }
    }
}

static void sp_cb_container_loaded(sp_playlistcontainer *container, void *userdata)
{
    struct spotifs_context* ctx = userdata;

    logger_message_color(ctx, logger_green, "%s\n", __func__);

    /* container was loaded, refresh playlists */
    pthread_mutex_lock(&g_directory.lock);
    initialize_playlists(ctx, container);
    pthread_mutex_unlock(&g_directory.lock);
}

static sp_playlistcontainer_callbacks pc_callbacks = {
    .container_loaded = &sp_cb_container_loaded,
};

static void sp_cb_offline_status_updated(sp_session *session)
{
    struct spotifs_context* ctx = sp_session_userdata(session);

    logger_message_color(ctx, logger_green, "%s: new status: %d\n", __func__, sp_session_connectionstate(session));
}

static void sp_cb_connectionstate_updated(sp_session* session)
{
    struct spotifs_context* ctx = sp_session_userdata(session);

    logger_message_color(ctx, logger_green, "%s: new status: %d\n", __func__, sp_session_connectionstate(session));
}

// handle logged in event
static void sp_cb_logged_in(sp_session *sess, sp_error error)
{
    struct spotifs_context* ctx = sp_session_userdata(sess);

    assert(ctx != NULL);
    assert(ctx->spotify_session == sess);

    pthread_mutex_lock(&ctx->lock);

    if (SP_ERROR_OK != error) {
        ctx->logged_in = -1;
        g_error("%s: got error: '%s'", __func__, sp_error_message(error));
    }
    else
    {
        ctx->logged_in = 1;
        g_debug("%s: logged in", __func__);
    }

    // signal that login is completed
    pthread_cond_signal(&ctx->change);
    pthread_mutex_unlock(&ctx->lock);

    ctx->spotify_playlist_container = sp_session_playlistcontainer(ctx->spotify_session);

    if (sp_playlistcontainer_is_loaded(ctx->spotify_playlist_container)) {
        sp_cb_container_loaded(ctx->spotify_playlist_container, ctx);
    } else {
        sp_playlistcontainer_add_callbacks(ctx->spotify_playlist_container, &pc_callbacks, ctx);
    }

    logger_message(ctx, "%s: exit\n", __func__);
}

static void sp_cb_logged_out(sp_session *sess)
{
    struct spotifs_context* ctx = sp_session_userdata(sess);
    g_debug("%s: logged out", __func__);

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

static void recreate_buffer(struct spotifs_context* ctx, struct track* track, off_t offset)
{
    free(track->buffer.data);

    track->buffer.offset = offset;
    track->buffer.size = 0;
    track->buffer.capacity = track->size - offset;
    track->buffer.data = calloc(1, track->buffer.capacity);
}

static int sp_cb_music_delivery(sp_session *session, const sp_audioformat *format, const void *frames, int num_frames)
{
    (void) session;

    struct spotifs_context *ctx = sp_session_userdata(session);

    pthread_mutex_lock(&current_track_mutex);

    if (!g_current_track) {
        pthread_mutex_unlock(&current_track_mutex);
        return num_frames;
    }

    if (!g_current_track->buffer.data)
    {
        /* 2 * 2 * 441 * (track->duration / 10);
         size is: 2 bytes (16 bit sample)  * channels * sample rate [sample/s] * duration [s] / 1000 */

        g_debug("%s: allocating buffer for %d seconds, track length is %dms",
            __func__, (int)(ceil(g_current_track->duration/1000.) + 1), g_current_track->duration);

        g_current_track->size = wave_size(2, format->channels, format->sample_rate, (ceil(g_current_track->duration/1000.) + 1));
        g_current_track->sample_rate = format->sample_rate;
        g_current_track->channels = format->channels;

        recreate_buffer(ctx, g_current_track, 0);

        g_debug("%s: allocating buffer: channels: %d, sample rate: %d, duration: %d, size: %d",
            __func__, format->channels, format->sample_rate, g_current_track->duration, g_current_track->size);
    }

    /* assume that these values can't change */
    assert(g_current_track->sample_rate == format->sample_rate);
    assert(g_current_track->channels == format->channels);

    size_t data_bytes = num_frames * 2 * format->channels;
    const size_t space_left = g_current_track->buffer.capacity - g_current_track->buffer.size;

    if (data_bytes > space_left) {
        g_warning("%s: write beyound the buffer, space left: %zubytes, data: %zubytes", __func__, space_left, data_bytes);
        data_bytes = space_left;
    }

    memcpy(g_current_track->buffer.data + g_current_track->buffer.size, frames, data_bytes);
    g_current_track->buffer.size += data_bytes;

    pthread_cond_signal(&current_track_cond);
    pthread_mutex_unlock(&current_track_mutex);

    return num_frames;
}

static void sp_cb_connection_error(sp_session *session, sp_error error)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);;

    logger_message_color(ctx, logger_red, "%s: %s\n", __func__, sp_error_message(error));
}

static void sp_cb_play_token_lost(sp_session *session)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);

    logger_message_color(ctx, logger_red, "%s: Play token lost\n", __func__);
}

static void sp_cb_log_message(sp_session *session, const char *data)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);

    /* logger_message_color(ctx, logger_green, "%s: %s\n", __FUNCTION__, data); */
}

static void sp_cb_end_of_track(sp_session *session)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);

    pthread_mutex_lock(&current_track_mutex);
    /* mark buffer as full */
    g_current_track->buffer.size = g_current_track->buffer.capacity;
    sp_session_player_play(ctx->spotify_session, 0);
    pthread_mutex_unlock(&current_track_mutex);

    logger_message_color(ctx, logger_green, "End of track\n");

}

static void streaming_error(sp_session *session, sp_error error)
{
    (void)session;
    struct spotifs_context *ctx = sp_session_userdata(session);

    logger_message_color(ctx, logger_green, "%s: %s\n", __func__, sp_error_message(error));
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
    .offline_status_updated = &sp_cb_offline_status_updated,
    .connectionstate_updated = &sp_cb_connectionstate_updated,
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
    logger_message(ctx, __func__);

    if (ctx->spotify_session || ctx->logged_in)
    {
        g_warning("%s: session already created. Exiting.", __func__);
        return -1;
    }

    sfs_add_child(&g_directory.first, "library", sfs_directory | sfs_container);

    spconfig.application_key_size = g_appkey_size;
    spconfig.userdata = ctx;

    sp_error err = sp_session_create(&spconfig, &ctx->spotify_session);

    if (SP_ERROR_OK != err)
    {
        g_critical("%s: sp_session_create failed: %s", __func__, sp_error_message(err));
        return -2;
    }

    pthread_mutexattr_init(&current_track_mutex_attr);
    pthread_mutexattr_settype(&current_track_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&current_track_mutex, &current_track_mutex_attr);

    /* we need to start worker thread at this point */
    if (0 != start_worker_thread(ctx))
    {
        logger_message(ctx, "%s: can't create worker thread\n", __func__);
        return -3;
    }

    ctx->logged_in = 2;
    sp_session_login(ctx->spotify_session, username, password, 0, NULL);

    pthread_mutex_lock(&ctx->lock);

    /* sp_session_login will probably request to notify worker thread
     * about new event to process. We need this lock now to properly
     * synchronize */

    while (2 == ctx->logged_in) {
        pthread_cond_wait(&ctx->change, &ctx->lock);
    }

    pthread_mutex_unlock(&ctx->lock);

    logger_message(ctx, "%s: exit\n", __func__);

    return ctx->logged_in;
}

void spotify_disconnect(struct spotifs_context* ctx)
{
    logger_message(ctx, __func__);

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

    if (g_current_track) {
        pthread_mutex_unlock(&current_track_mutex);
        return -1;
    }

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

    sp_session_player_play(ctx->spotify_session, 0); /* pause and unload */
    sp_session_player_unload(ctx->spotify_session);

    free(g_current_track->buffer.data);
    g_current_track->buffer.data = NULL;
    g_current_track = NULL;

    pthread_mutex_unlock(&current_track_mutex);
}

static size_t bytes_per_second(struct track* track)
{
    return track->sample_rate * track->channels * 2;
}

static size_t offset_to_second(struct track* track, off_t offset)
{
    if (offset)
        return offset / bytes_per_second(track);
    else
        return 0;
}

int spotify_read(struct spotifs_context* ctx, struct track* track, off_t offset, size_t size, char *buffer)
{
    int copied = 0;
    int current_second;

    pthread_mutex_lock(&current_track_mutex);

    g_debug("%s: read(%zu, %zu), buffer(%zu, %zu)\n", __func__, offset, size, g_current_track->buffer.offset, g_current_track->buffer.size);

    /* wait for any data, proper size will be calculated after first data arrive */
    while(!g_current_track->buffer.data) {
        pthread_cond_wait(&current_track_cond, &current_track_mutex);
    }

    if (offset >= track->size) {
        pthread_mutex_unlock(&current_track_mutex);
        return 0;
    }

    if (offset + size >= track->size) {
        size = track->size - offset;
    }

    /* copy header if needed */
    if (offset < wave_header_size()) {
        if (offset + size < wave_header_size()) {
            /* read only in header */
            memcpy(buffer, wave_standard_header(g_current_track->size) + offset, size);
            pthread_mutex_unlock(&current_track_mutex);
            return size;
        } else {
            /* read exceed header */
            copied = wave_header_size() - offset;

            memcpy(buffer, wave_standard_header(g_current_track->size) + offset, copied);

            buffer += copied;
            offset = wave_header_size();
            size -= copied;
        }
    } else {
        offset -= wave_header_size();
    }

    current_second = offset_to_second(g_current_track, offset);

    /* detect seek:
     * seek back if read offset is less than buffer offset,
     * seek forward if read offset is at least ~2 seconds later than current buffer offset */
    if (offset < g_current_track->buffer.offset)
    {
        g_debug("need to seek back, current offset is %zu, new offset is %zu, seek to: %i", g_current_track->buffer.offset, offset, current_second * 1000);
        recreate_buffer(ctx, g_current_track, current_second * bytes_per_second(track));
        sp_session_player_seek(ctx->spotify_session, current_second * 1000);
    }
    else if (offset > g_current_track->buffer.offset + g_current_track->buffer.size + (1024 * 1024))
    {

        g_debug("need to seek forward, current offset is %zu, new offset is %zu, seek to: %i", g_current_track->buffer.offset, offset, current_second * 1000);
        recreate_buffer(ctx, g_current_track, current_second * bytes_per_second(track));
        sp_session_player_seek(ctx->spotify_session, current_second * 1000);
    }

    /* wait for data if needed */
    while((offset + size > g_current_track->buffer.offset + g_current_track->buffer.size)) {
        pthread_cond_wait(&current_track_cond, &current_track_mutex);
    }

    /* copy remaining part from buffer */
    if (offset + size <= g_current_track->buffer.offset + g_current_track->buffer.size) {
        memcpy(buffer, g_current_track->buffer.data + (offset - g_current_track->buffer.offset), size);
        copied = size;
    }

    logger_message_color(ctx, logger_red, "%s\n", __func__);

    pthread_mutex_unlock(&current_track_mutex);

    return copied;
}

struct track* spotify_current(struct spotifs_context* ctx)
{
    return g_current_track;
}
