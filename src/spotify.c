#include "spotify.h"
#include "spotify_appkey.h"
#include "logger.h"

#include <pthread.h>
#include <string.h>
#include <assert.h>

static pthread_mutex_t login_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mainloop_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t player_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t login_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t mainloop_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t player_cond = PTHREAD_COND_INITIALIZER;

// session callbacks
static struct playlist* sp_user_playlists = NULL;

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

static void initialize_playlists(sp_playlistcontainer *container)
{
    struct spotifs_context* ctx = get_global_context;

    // get playlist container and calculate number of current plylists
    const int num_playlists = sp_playlistcontainer_num_playlists(container);

    if (0 == num_playlists)
    {
        sp_user_playlists = NULL;
        return;
    }

    sp_user_playlists = calloc(1, sizeof(struct playlist));
    struct playlist *current_playlist = sp_user_playlists;

    int i, j;
    for (i = 0; i < num_playlists; ++i)
    {
        // fetch playlist from spotify
        sp_playlist *playlist =	sp_playlistcontainer_playlist (container, i);

        // fetch number of tracks per list
        int num_tracks = sp_playlist_num_tracks(playlist);

        // copy playlist name
        current_playlist->title = strdup(sp_playlist_name(playlist));
        replace_char(current_playlist->title, '/', '\\');

        if (num_tracks)
        {
            // allocate first track
            current_playlist->tracks = calloc(1, sizeof(struct track));
            struct track* current_track = current_playlist->tracks;

            for (j = 0; j < num_tracks; ++j)
            {
                sp_track *track = sp_playlist_track(playlist, j);
                current_track->title = strdup(sp_track_name(track));
                current_track->duration = sp_track_duration(track);
                current_track->spotify_track = track;
                current_track->size = 0;

                replace_char(current_track->title, '/', '\\');

                if (j < num_tracks-1)
                {
                    current_track->next = calloc(1, sizeof(struct track));
                    current_track = current_track->next;
                }

                // TODO: do proper cleanup of resources!!!!!
                //sp_track_release(track);
            }
        }
        else
        {
            current_playlist->tracks = NULL;
        }

        // allocate next playlist
        if (i < num_playlists-1)
        {
            current_playlist->next = calloc(1, sizeof(struct playlist));
            current_playlist = current_playlist->next;
        }

        sp_playlist_release(playlist);
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


static int estimated_bps = 0;

int sp_cb_music_delivery(sp_session *session, const sp_audioformat *format, const void *frames, int num_frames)
{
    struct spotifs_context *ctx = get_global_context;
    logger_message(ctx, "sp_cb_music_delivery: enter\n");

    // for now, only function of this method is to caluclate final audio size
    estimated_bps = 2 /* int16 (2 bytes) frame */ * format->channels * format->sample_rate;

    // notify caller
    logger_message(ctx, "sp_cb_music_delivery: notify and leave\n");
    pthread_cond_signal(&player_cond);

    return 0;
}

static sp_session_callbacks session_callbacks = {
    .logged_in = &sp_cb_logged_in,
    .logged_out = &sp_cb_logged_out,
    .notify_main_thread = &sp_cb_notify_main_thread,
    .music_delivery = &sp_cb_music_delivery,
    NULL,
    /*
    .music_delivery = &music_delivery,
    .metadata_updated = &metadata_updated,
    .play_token_lost = &play_token_lost,
    .log_message = NULL,
    .end_of_track = &end_of_track,*/
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
    if (sp_user_playlists == NULL)
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

    return (const struct playlist*) sp_user_playlists;
}

int spotify_calculate_filezsize(struct spotifs_context* ctx, struct track* track)
{
    // clear bps
    estimated_bps = 0;
    sp_error err;

    // load and play
    if(SP_ERROR_OK != (err = sp_session_player_load(ctx->spotify_session, track->spotify_track)))
    {
        logger_message(ctx, "spotify_calculate_filezsize: sp_session_player_load: %s\n", sp_error_message(err));

        track->size = -1;
        return -1;
    }

    if (SP_ERROR_OK != (err = sp_session_player_play(ctx->spotify_session, 1)))
    {
        logger_message(ctx, "spotify_calculate_filezsize: sp_session_player_play: %s\n", sp_error_message(err));

        track->size = -1;
        return -1;
    }

    pthread_mutex_lock(&player_mutex);

    if (estimated_bps <= 0)
        pthread_cond_wait(&player_cond, &player_mutex);

    track->size = estimated_bps * (track->duration / 1000);

    // stop and upload
    sp_session_player_play(ctx->spotify_session, 0);
    sp_session_player_unload(ctx->spotify_session);

    pthread_mutex_unlock(&player_mutex);

    return track->size;
}
