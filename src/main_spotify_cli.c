/* Interactive test application for spotify subsystem */

#include <glib.h>
#include <string.h>
#include "spotify.h"
#include "sfs.h"

static gchar* g_login = NULL;
static gchar* g_password = NULL;
static gboolean g_verbose = FALSE;

static GOptionEntry entries[] = {
    { "user", 'u', 0, G_OPTION_ARG_STRING, &g_login, "spotify user name", NULL},
    { "password", 'p', 0, G_OPTION_ARG_STRING, &g_password, "spotify password", NULL},
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &g_verbose, "Be verbose", NULL },
    { NULL }
};

int main(int argc, char **argv)
{
    GError *error = NULL;
    GOptionContext *context;
    struct spotifs_context spotify_context = {0};
    char command[512];
    char* args[3];
    gboolean running = TRUE;

    context = g_option_context_new("- test tree model performance");
    g_option_context_add_main_entries(context, entries, NULL);

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        return -1;
    }

    if (!g_login || !g_password) {
        g_print("login and/or password is missing\n");
        return -1;
    }

    if (spotify_connect(&spotify_context, g_login, g_password) < 0) {
        g_print("Can't connect to spotify service\n");
        return -1;
    }

    while (running) {
        int i;

        /* paese user input */
        fgets(command, sizeof(command), stdin);
        command[strlen(command) - 1] = 0;

        strtok(command, " ");
        for (i = 0; i < 3; i ++) {
            args[i] = strtok(NULL, " ");
        }

        if (!strcmp(command, "list")) {
            int index = 1;
            int list_index = -1;
            struct sfs_entry* list = spotify_get_playlists();

            if (args[0] != NULL) list_index = atoi(args[0]);

            if (list_index > 0) {
                list = sfs_get_child_by_index(list, list_index - 1)->children;

                while (list) {
                    g_print("Song %d: %s\n", index, list->name);
                    index ++;
                    list = list->next;
                }
            } else {
                list = list->children;

                while (list) {
                    g_print("Playlist %d: %s\n", index, list->name);
                    index ++;
                    list = list->next;
                }
            }
        } else if (!strcmp(command, "exit")) {
            running = FALSE;
        } else if (!strcmp(command, "load")) {
            struct sfs_entry* list = spotify_get_playlists();
            int playlist, song;

            if (!args[0] || !args[1]) {
                g_print("load [playlist] [song]\n");
                continue;
            }

            playlist = atoi(args[0]);
            song = atoi(args[1]);

            list = sfs_get_child_by_index(list, playlist - 1);
            list = sfs_get_child_by_index(list, song - 1);

            g_print("starting download of %s\n", list->name);
            spotify_buffer_track(&spotify_context, list->track);
        } else if (!strcmp(command, "watch")) {
            struct track* current = spotify_current(&spotify_context);

            while (current->buffer.offset + current->buffer.size < current->size) {
                g_print("Buffer, offset: %zu, size: %zu, track size: %d\n", current->buffer.offset, current->buffer.size, current->size);
                sleep(1);
            }
        }
    }

    spotify_disconnect(&spotify_context);
    return 0;
}