#ifndef SPOTIFS_PATH_OPERATIONS_H
#define SPOTIFS_PATH_OPERATIONS_H

#include "spotify.h"

struct track* get_track_from_library(struct spotifs_context* ctx, const char* path);
const char** get_root_layout();

bool is_library_playlist_path(const char* path);
bool is_path_to_library_track(const char* path);
bool is_library_path(const char *directory);
bool is_path_in_root(const char *directory);

#endif // PATH_OPERATIONS_H
