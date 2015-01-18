#ifndef SPOTIFS_CONTEXT_H
#define SPOTIFS_CONTEXT_H

#include <stdio.h>
#include <libspotify/api.h>
#include <fuse.h>

struct spotifs_context
{
    FILE* logfile;
    bool logged_in;
    bool worker_thread_running;
    sp_session* spotify_session;
    sp_playlistcontainer* spotify_playlist_container;
};

extern struct spotifs_context g_context;

// define accessor macro for global context
#define get_global_context &g_context;

#endif // SPOTIFS_CONTEXT_H
