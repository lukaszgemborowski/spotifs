#ifndef SPOTIFS_WAVE_H
#define SPOTIFS_WAVE_H

#include <stddef.h>
#include <stdint.h>

size_t wave_header_size();
size_t wave_size(int bytes, int channels, int rate, int seconds);
const char* wave_standard_header(int32_t data_size);

#endif //SPOTIFS_WAVE_H
