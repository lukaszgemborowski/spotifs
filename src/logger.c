#include "logger.h"
#include <glib.h>
#include <stdio.h>

static FILE* log_file = NULL;
static gboolean close_fd = FALSE;

static
void log_handler(
    const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
    fprintf(log_file, "%s\n", message);
}

void logger_set_file(const char* filename)
{
    log_file = fopen(filename, "a");

    if (!log_file)
    {
        g_critical("Can't open logfile '%s'", filename);
    }
    else
    {
        close_fd = TRUE;
        g_log_set_handler(NULL, G_LOG_LEVEL_MASK, log_handler, NULL);
    }
}

void logger_set_stream(FILE *fd)
{
    log_file = fd;
    close_fd = FALSE;
    g_log_set_handler(NULL, G_LOG_LEVEL_MASK, log_handler, NULL);
}

void logger_stop()
{
    if (log_file && close_fd) fclose(log_file);
}
