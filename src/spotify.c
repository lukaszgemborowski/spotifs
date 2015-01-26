#include "spotify.h"
#include "spotify_appkey.h"
#include "logger.h"

#include <pthread.h>
#include <string.h>
#include <assert.h>

static pthread_mutex_t login_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mainloop_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t player_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t current_track_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t login_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t mainloop_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t player_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t current_track_cond = PTHREAD_COND_INITIALIZER;

static struct playlist* g_playlists = NULL;

// worker thread variables
static bool spotify_running = 0;
static bool event_available = 0;
static bool event_disconnect = 0;
static pthread_t spotify_worker_thread_handle;

// spotify worker thread
static void* spotify_worker_thread(void *param)
{
    (void) param;

    int next_timeout;
    sp_error err;
    struct spotifs_context* ctx = get_global_context;

    while (spotify_running)
    {
        // wait for new event to be available
        pthread_mutex_lock(&mainloop_mutex);

        if (!event_available)
        {
            pthread_cond_wait(&mainloop_cond, &mainloop_mutex);
        }

        // clear event flag before processing event and unlocking mutex
        event_available = 0;

        // copy disconnect flag to stack and clear it
        bool call_logout = event_disconnect;
        event_disconnect = 0;

        pthread_mutex_unlock(&mainloop_mutex);

        // process spotify event
        err = sp_session_process_events(ctx->spotify_session, &next_timeout);

        // TODO: next_timeout should be considered
        if (SP_ERROR_OK != err)
        {
            logger_message(ctx, "spotify_worker_thread: error: %s.\n", sp_error_message(err));
        }

        if (call_logout)
        {
            // logout request
        #if 0
            // SIGSEGV after calling logout, WTF?
            sp_session_logout(ctx->spotify_session);
        #endif
        }
    }
}

static void replace_char(char *string, char a, char b)
{
    while(*string)
    {
        if (*string == a)
            *string = b;

        string ++;
    }
}

static void recreate_playlist(struct playlist* playlist)
{
    // re-fetch playlist name
    free (playlist->title);
    playlist->title = strdup(sp_playlist_name(playlist->spotify_playlist));
    replace_char(playlist->title, '/', '\\');

    // free already allocated tracks
    struct track* track = NULL;
    for (track = playlist->tracks; track != NULL; track = track->next)
    {
        // free track title
        free(track->title);

        // release memory allocated for track
        free(track);
    }

    playlist->tracks = NULL;

    // read number of tracks
    int num_tracks = sp_playlist_num_tracks(playlist->spotify_playlist);

    if (num_tracks > 0)
    {
        // allocate first track
        playlist->tracks = calloc(1, sizeof(struct track));
        track = playlist->tracks;
        int i = 0;

        for (i = 0; i < num_tracks; i ++)
        {
            sp_track *s_track = sp_playlist_track(playlist->spotify_playlist, i);
            track->title = strdup(sp_track_name(s_track));
            track->duration = sp_track_duration(s_track);
            track->spotify_track = s_track;
            track->size = 0;

            replace_char(track->title, '/', '\\');

            if (i < num_tracks-1)
            {
                track->next = calloc(1, sizeof(struct track));
                track = track->next;
            }
        }
    }
}

void sp_cb_playlist_metadata_updated(sp_playlist *pl, void *userdata)
{
    (void *)pl;
    // forward call to recreate_playlist, basically we need to refresh all
    // playlist data, including tracks
    recreate_playlist(userdata);
}

static sp_playlist_callbacks pl_callbacks = {
    .playlist_metadata_updated = &sp_cb_playlist_metadata_updated
};

static void initialize_playlists(sp_playlistcontainer *container)
{
    struct spotifs_context* ctx = get_global_context;

    // get playlist container and calculate number of current plylists
    const int num_playlists = sp_playlistcontainer_num_playlists(container);

    if (0 == num_playlists)
    {
        g_playlists = NULL;
        return;
    }

    g_playlists = calloc(1, sizeof(struct playlist));
    struct playlist *current_playlist = g_playlists;

    int i = 0;
    for (i = 0; i < num_playlists; ++i)
    {
        // fetch playlist from spotify
        sp_playlist *playlist =	sp_playlistcontainer_playlist (container, i);

        // save spotify playlist
        current_playlist->spotify_playlist = playlist;

        // recreate playlist
        recreate_playlist(current_playlist);

        // register callbacks
        sp_playlist_add_callbacks(playlist, &pl_callbacks, current_playlist);

        // allocate next playlist
        if (i < num_playlists-1)
        {
            current_playlist->next = calloc(1, sizeof(struct playlist));
            current_playlist = current_playlist->next;
        }

        // TODO: release
        // sp_cb_playlist_metadata_updated
    }

}

static void sp_cb_container_loaded(sp_playlistcontainer *container, void *userdata)
{
    (void)userdata;

    struct spotifs_context* ctx = get_global_context;

    logger_message(ctx, "sp_cb_container_loaded\n");

    // container was loaded, refresh playlists
    pthread_mutex_lock(&login_mutex);
    initialize_playlists(container);
    pthread_cond_signal(&login_cond);
    pthread_mutex_unlock(&login_mutex);
}

static sp_playlistcontainer_callbacks pc_callbacks = {
    .container_loaded = &sp_cb_container_loaded,
};

// handle logged in event
static void sp_cb_logged_in(sp_session *sess, sp_error error)
{
    struct spotifs_context* ctx = get_global_context;

    assert(ctx != NULL);
    assert(ctx->spotify_session == sess);

    pthread_mutex_lock(&login_mutex);
    if (SP_ERROR_OK != error)
    {
        ctx->logged_in = 0;

        logger_message(ctx, "sp_cb_logged_in: got error: %s\n", sp_error_message((error)));
    }
    else
    {
        ctx->logged_in = 1;

        logger_message(ctx, "sp_cb_logged_in: Logged in\n");
    }

    // get and save playlist container
    ctx->spotify_playlist_container = sp_session_playlistcontainer(sess);

    // register callbacks
    sp_playlistcontainer_add_callbacks(ctx->spotify_playlist_container, &pc_callbacks, NULL);

    // signal that login is completed
    pthread_cond_signal(&login_cond);
    pthread_mutex_unlock(&login_mutex);
}

// handle logged out event
static void sp_cb_logged_out(sp_session *sess)
{
    struct spotifs_context* ctx = get_global_context;
    logger_message(ctx, "sp_cb_logged_out: Logged out.\n");

    pthread_mutex_lock(&mainloop_mutex);
    spotify_running = 0;
    event_available = 1;

    pthread_cond_signal(&mainloop_cond);
    pthread_mutex_unlock(&mainloop_mutex);
}

// handle main thread notification
static void sp_cb_notify_main_thread(sp_session *session)
{
    (void) session;

    pthread_mutex_lock(&mainloop_mutex);
    event_available = 1;
    pthread_cond_signal(&mainloop_cond);
    pthread_mutex_unlock(&mainloop_mutex);
}

static struct track* g_current_track = NULL;

static int sp_cb_music_delivery(sp_session *session, const sp_audioformat *format, const void *frames, int num_frames)
{
    (void) session;

    if (!g_current_track)
        return num_frames;

    struct spotifs_context *ctx = get_global_context;
    //logger_message(ctx, "sp_cb_music_delivery: enter channels: %d, sample_rate: %d\n", format->channels, format->sample_rate);

    if (NULL == g_current_track->buffer)
    {
        logger_message(ctx, "sp_cb_music_delivery: allocating buffer: channels: %d, sample rate: %d\n", format->channels, format->sample_rate);

        // 2 * 2 * 441 * (track->duration / 10);
        // size is: 2 bytes (16 bit sample)  * channels * sample rate [sample/s] * duration [s] / 1000

        g_current_track->size = 2 * format->channels * (format->sample_rate / 100) * (g_current_track->duration / 10);
        g_current_track->buffer = malloc(g_current_track->size);
        g_current_track->buffer_pos = 0;
    }

    const size_t data_bytes = num_frames * 2 * format->channels;
    memcpy(&g_current_track->buffer[g_current_track->buffer_pos], frames, data_bytes);
    g_current_track->buffer_pos += data_bytes;

    pthread_cond_signal(&current_track_cond);

    return num_frames;
}

static void sp_cb_connection_error(sp_session *session, sp_error error)
{
    (void)session;
    struct spotifs_context *ctx = get_global_context;

    logger_message(ctx, "%s: %s\n", __FUNCTION__, sp_error_message(error));
}

static void sp_cb_play_token_lost(sp_session *session)
{
    (void)session;
    struct spotifs_context *ctx = get_global_context;

    logger_message(ctx, "Play token lost\n");
}

static void sp_cb_log_message(sp_session *session, const char *data)
{
    (void)session;
    struct spotifs_context *ctx = get_global_context;

    logger_message(ctx, "%s: %s\n", __FUNCTION__, data);
}

static void sp_cb_end_of_track(sp_session *session)
{
    (void)session;
    struct spotifs_context *ctx = get_global_context;

    logger_message(ctx, "End of track\n");
}

static void streaming_error(sp_session *session, sp_error error)
{
    (void)session;
    struct spotifs_context *ctx = get_global_context;

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

int start_worker_thread()
{
    assert(spotify_running == 0);

    // set global controlling variables to 1 and start worker thread
    spotify_running = 1;
    event_available = 1;

    int ret = pthread_create(&spotify_worker_thread_handle, NULL, spotify_worker_thread, NULL);

    if (ret)
    {
        // clear variables on error
        spotify_running = 0;
        event_available = 0;
    }

    return ret;
}

void stop_worker_thread()
{
    assert(spotify_running == 1);

    spotify_running = 0;

    // notify main thread about "new event", this will
    // cause exit from conditional wait, one more loop and thread termination
    pthread_mutex_lock(&mainloop_mutex);
    event_available = 1;
    pthread_cond_signal(&mainloop_cond);
    pthread_mutex_unlock(&mainloop_mutex);

    pthread_join(spotify_worker_thread_handle, NULL);
}

int spotify_connect(struct spotifs_context* ctx, const char *username, const char *password)
{
    if (ctx->spotify_session || ctx->logged_in)
    {
        logger_message(ctx, "spotify_connect: session already created. Exiting.\n");
        return -1;
    }

    spconfig.application_key_size = g_appkey_size;

    // create spotify session
    sp_error sperr = sp_session_create(&spconfig, &ctx->spotify_session);

    if (SP_ERROR_OK != sperr)
    {
        logger_message(ctx, "spotify_connect: sp_session_create failed: %s\n", sp_error_message(sperr));
        return -2;
    }

    // we need to start worker thread at this point
    if (0 != start_worker_thread())
    {
        logger_message(ctx, "spotify_connect: can't create worker thread\n");
        return -3;
    }

    // login to spotify, this is asynchronous so we need to synchronise this call
    // via mutex and conditional variable
    sp_session_login(ctx->spotify_session, username, password, 0, NULL);

    pthread_mutex_lock(&login_mutex);

    // check if we are logged in
    if (ctx->logged_in != 1)
    {
        // if not, wait for signal
        pthread_cond_wait(&login_cond, &login_mutex);
    }

    // wait for playlist container to load
    if (g_playlists == NULL)
    {
        pthread_cond_wait(&login_cond, &login_mutex);
    }

    pthread_mutex_unlock(&login_mutex);

    // ctx->logged_in should be set by now in login callback, we can use it here
    return ctx->logged_in?0:-1;
}

void spotify_disconnect(struct spotifs_context* ctx)
{
    if (!ctx->spotify_session || !ctx->logged_in)
    {
        logger_message(ctx, "spotify_connect: session is not created.\n");
        return;
    }

    // request disconnection and wait for thread to join
    event_disconnect = 1;
    stop_worker_thread();
}

const struct playlist* spotify_get_user_playlists(struct spotifs_context* ctx)
{
    (void) ctx;

    return (const struct playlist*) g_playlists;
}

int spotify_buffer_track(struct spotifs_context* ctx, struct track* track)
{
    logger_message(ctx, "%s\n", __FUNCTION__);

    assert(g_current_track == NULL);
    sp_error err;
    int ret = 0;

    pthread_mutex_lock(&current_track_mutex);
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
