#ifndef SPOTIFS_CONTEXT_H
#define SPOTIFS_CONTEXT_H

#include <stdio.h>
#include <libspotify/api.h>
#include <pthread.h>

struct spotifs_context
{
    FILE* logfile;
    int logged_in;
    int worker_running;
    int spotify_event;
    sp_session* spotify_session;
    sp_playlistcontainer* spotify_playlist_container;

    pthread_mutex_t lock;
    pthread_cond_t change;
};

#endif // SPOTIFS_CONTEXT_H
