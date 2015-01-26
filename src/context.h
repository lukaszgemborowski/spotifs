#ifndef SPOTIFS_CONTEXT_H
#define SPOTIFS_CONTEXT_H

#include <stdio.h>
#include <libspotify/api.h>


struct spotifs_context
{
    FILE* logfile;
    bool logged_in;
    bool worker_thread_running;
    sp_session* spotify_session;
    sp_playlistcontainer* spotify_playlist_container;
};

#endif // SPOTIFS_CONTEXT_H
