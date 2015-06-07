#include "wave.h"
#include <assert.h>

struct wave_header
{
    char mark[4];
    int32_t overall_size;
    char wave[4];
    char fmt[4];
    int32_t format_len;
    int16_t format;
    int16_t channels;
    int32_t samplerate;
    int32_t samplerate2;
    int16_t channelrate;
    int16_t bitspersample;
    char data[4];
    int32_t datasize;
} __attribute__((packed));

static struct wave_header static_header = {
        .mark = {'R', 'I', 'F', 'F'},
        // .filesize - set in spotifs_read
        .wave = {'W', 'A', 'V', 'E'},
        .fmt = {'f', 'm', 't', ' '},
        .format_len = 16,
        .format = 1,
        .channels = 2,
        .samplerate = 44100,
        .samplerate2 = 176400,
        .channelrate = 4,
        .bitspersample = 16,
        .data = {'d', 'a', 't', 'a'},
        .datasize = 0
};

size_t wave_header_size()
{
    return sizeof(static_header);
}

const char* wave_standard_header(int32_t data_size)
{
    static_header.datasize = data_size;
    static_header.overall_size = data_size + wave_header_size() - 8;

    return (const char*)&static_header;
}

size_t wave_size(int bytes, int channels, int rate, int ms)
{
    assert (!(ms % 1000));
    return bytes * channels * rate * (ms / 1000);
}
