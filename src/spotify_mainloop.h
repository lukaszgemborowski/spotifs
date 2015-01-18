#ifndef SPOTIFS_SPOTIFY_MAINLOOP_H
#define SPOTIFS_SPOTIFY_MAINLOOP_H

int spotify_mainloop_start();
void sporify_mainloop_stop();

void spotify_push_login(const char *user, const char *password);
void spotify_push_logout();

#endif // SPOTIFS_SPOTIFY_MAINLOOP_H
