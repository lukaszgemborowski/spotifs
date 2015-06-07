#include "buffer.c"
#include "minunit.h"

char* test_create_and_release_buffer()
{
    struct stream_buffer* buffer = buffer_create(1024);

    mu_assert("buffer created", buffer);
    mu_assert("buffer have proper chunk size", buffer->chunk_size == 1024);

    buffer_release(buffer);
    return 0;
}

char* test_add_small_data()
{
    struct stream_buffer* buffer = buffer_create(20);
    const char* test_data = "test string";
    char input_buffer[20];

    buffer_append_to(buffer, test_data, strlen(test_data));
    buffer_copy_from(buffer, input_buffer, 0, strlen(test_data));

    mu_assert("1 chunk", buffer->chunks == 1);
    mu_assert("11 bytes", buffer->size == 11);
    mu_assert("full data correct", !strcmp(test_data, input_buffer));

    buffer_release(buffer);

    return 0;
}

char* test_add_big_data()
{
    struct stream_buffer* buffer = buffer_create(10);
    const char* test_data = "much more longer than 10 byte string, must span multiple chunks of data";
    char* input = malloc(strlen(test_data) + 1);

    buffer_append_to(buffer, test_data, strlen(test_data));
    buffer_copy_from(buffer, input, 0, strlen(test_data));

    mu_assert("test_add_big_data: buffer->size == strlen(test_data)", buffer->size == strlen(test_data));
    mu_assert("test_add_big_data: !strcmp(test_data, input)", !strcmp(test_data, input));

    buffer_release(buffer);

    return 0;
}

char* test_read_partial_data()
{
    struct stream_buffer* buffer = buffer_create(10);
    const char* test_data = "12345678901234567890"; /* two chunks */
    char* input = malloc(21);
    int copied;

    buffer_append_to(buffer, test_data, 20);
    copied = buffer_copy_from(buffer, input, 0, 40);

    mu_assert("test_read_partial_data: buffer->size == strlen(test_data)", buffer->size == strlen(test_data));
    mu_assert("test_read_partial_data: !strcmp(test_data, input)", !strcmp(test_data, input));
    mu_assert("test_read_partial_data: copied == 20", copied == 20);

    buffer_release(buffer);

    return 0;
}

char* test_read_data_at_offset()
{
    struct stream_buffer* buffer = buffer_create(5);
    const char* test_data = "12345123451234512345";
    char* input = malloc(21);
    int copied;

    buffer_append_to(buffer, test_data, 20);
    copied = buffer_copy_from(buffer, input, 5, 11);

    mu_assert("test_read_data_at_offset: buffer->size == strlen(test_data)", buffer->size == strlen(test_data));
    mu_assert("test_read_data_at_offset: !strncmp(test_data + 5, input, 11)", !strncmp(test_data + 5, input, 11));
    mu_assert("test_read_data_at_offset: copied == 11", copied == 11);

    buffer_release(buffer);

    return 0;
}

char* test_read_data_over_the_end()
{
    struct stream_buffer* buffer = buffer_create(10);
    const char* test_data = "123456789012345678901234567890123456789012345678901234567890"; /* 60 bytes */
    char* input = malloc(61);
    int copied;

    buffer_append_to(buffer, test_data,60);
    copied = buffer_copy_from(buffer, input, 0, 1024);

    mu_assert("test_read_data_over_the_end: buffer->size == strlen(test_data)", buffer->size == strlen(test_data));
    mu_assert("test_read_data_over_the_end: !strcmp(test_data, input)", !strcmp(test_data, input));
    mu_assert("test_read_data_over_the_end: copied == 60", copied == 60);

    buffer_release(buffer);

    return 0;
}

char* test_read_data_at_offset_and_over_the_end()
{
    struct stream_buffer* buffer = buffer_create(10);
    const char* test_data = "123456789012345678901234567890123456789012345678901234567890"; /* 60 bytes */
    char* input = malloc(60 + 1);
    int copied;

    buffer_append_to(buffer, test_data, 60);
    copied = buffer_copy_from(buffer, input, 5, 1100);

    mu_assert("test_read_data_at_offset_and_over_the_end: buffer->size == 60", buffer->size == 60);
    mu_assert("test_read_data_at_offset_and_over_the_end: !strcmp(test_data + 5, input)", !strcmp(test_data + 5, input));
    mu_assert("test_read_data_at_offset_and_over_the_end: copied == 55", copied == 55);

    buffer_release(buffer);

    return 0;
}

