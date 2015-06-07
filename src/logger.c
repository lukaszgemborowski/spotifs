#include "logger.h"
#include <errno.h>
#include <stdarg.h>

#define C_GREEN  "\x1B[32m"
#define C_RED    "\x1B[31m"
#define C_NORMAL "\x1B[0m"

static const char* color_map[] = {
    [logger_red] = C_RED,
    [logger_green] = C_GREEN
};

int logger_open(struct spotifs_context* ctx)
{
    ctx->logfile = fopen("spotifs.log", "w");

    if (ctx->logfile)
    {
        logger_message(ctx, "Logger created\n");
        return 0;
    }
    else
    {
        logger_message(ctx, "Logger cannot be created\n");
        return -errno;
    }
}

void logger_close(struct spotifs_context* ctx)
{
    if (ctx->logfile)
    {
        fclose(ctx->logfile);
        ctx->logfile = NULL;
    }
}

static void write_log(FILE* file,  va_list arguments, const char* format)
{
    vfprintf(file, format, arguments);
}

void logger_message(struct spotifs_context* ctx, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    if (ctx->logfile)
        write_log(ctx->logfile, ap, format);
    else
        write_log(stderr, ap, format);

    va_end(ap);
}

void logger_message_color(struct spotifs_context* ctx, enum logger_color color, const char *format, ...)
{
    logger_message(ctx, "%s", color_map[color]);

    va_list ap;
    va_start(ap, format);

    if (ctx->logfile)
        write_log(ctx->logfile, ap, format);
    else
        write_log(stderr, ap, format);

    va_end(ap);

    logger_message(ctx, "%s", C_NORMAL);
}
