#ifndef SPOTIFS_LOGGER_H
#define SPOTIFS_LOGGER_H

#include "context.h"

enum logger_color {
    logger_green,
    logger_red,
};

int logger_open(struct spotifs_context* ctx);
void logger_close(struct spotifs_context* ctx);
void logger_message(struct spotifs_context* ctx, const char *format, ...);
void logger_message_color(struct spotifs_context* ctx, enum logger_color color, const char *format, ...);

#endif // SPOTIFS_LOGGER_H
