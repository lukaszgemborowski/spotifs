#include "logger.h"
#include <errno.h>
#include <stdarg.h>

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

void logger_message(struct spotifs_context* ctx, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    if (ctx->logfile)
        vfprintf(ctx->logfile, format, ap);
    else
        vfprintf(stderr, format, ap);
}
