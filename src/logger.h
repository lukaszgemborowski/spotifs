#ifndef SPOTIFS_LOGGER_H
#define SPOTIFS_LOGGER_H

#include <stdio.h>

void logger_set_file(const char* filename);
void logger_set_stream(FILE *fd);
void logger_stop();

#endif // SPOTIFS_LOGGER_H
