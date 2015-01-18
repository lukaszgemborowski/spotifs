#ifndef SPOTIFS_LOGGER_H
#define SPOTIFS_LOGGER_H

#include "context.h"

int logger_open(struct spotifs_context* ctx);
void logger_close(struct spotifs_context* ctx);
void logger_message(struct spotifs_context* ctx, const char *format, ...);

#endif // SPOTIFS_LOGGER_H
