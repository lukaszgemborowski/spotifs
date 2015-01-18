#include "spotify_mainloop.h"
#include "context.h"
#include <pthread.h>
/*
static bool running = 0;
static bool spotify_event = 0;
static bool user_event = 0;

static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t event_condition = PTHREAD_COND_INITIALIZER;
static pthread_t main_loop_thread;

static void spotify_mainloop()
{
    struct spotifs_context* ctx = get_global_context;
    int next_timeout;

    while (running)
    {
        pthread_mutex_lock(&event_mutex);

        if (spotify_event == 0 && user_event == 0)
        {
            pthread_cond_wait(&event_condition, &event_mutex);
        }

        bool process_spotify_event = spotify_event;
        bool process_user_event = user_event;

        pthread_mutex_unlock(&event_mutex);

        // process spotify event
        if (process_spotify_event)
        {
            sp_error err = sp_session_process_events(ctx->spotify_session, &next_timeout);

            if (SP_ERROR_OK != err)
            {
                logger_message(ctx, "spotify_mainloop: error: %s.\n", sp_error_message(err));
            }
        }

        // process user event
        if (process_user_event)
        {
            // process user event
        }
    }

    // logout from spotify
}

// spotify worker thread wrapper
static void* spotify_worker_thread(void *param)
{
    (void) param;
    spotify_mainloop();
}

int spotify_mainloop_start()
{
    assert(running == 0);

    running = 1;
    spotify_event = 1;

    int ret = pthread_create(&main_loop_thread, NULL, spotify_worker_thread, NULL);

    if (ret)
    {
        running = 0;
    }

    return ret;
}

void sporify_mainloop_stop()
{
}

void spotify_push_login(const char *user, const char *password);
void spotify_push_logout();


*/
