#ifndef SPOTIFS_TESTS_H
#define SPOTIFS_TESTS_H

#include "minunit.h"

char* test_is_path_under();

char* test_add_one_child();
char* test_sfs_add_directory();
char* test_get_root();
char* test_get_subdirectory();
char* test_get_subdirectory_level_three();

char* test_create_and_release_buffer();
char* test_add_small_data();
char* test_add_big_data();
char* test_read_partial_data();
char* test_read_data_at_offset();
char* test_read_data_over_the_end();
char* test_read_data_at_offset_and_over_the_end();

char* all_tests()
{
    mu_run_test(test_is_path_under);
    mu_run_test(test_add_one_child);
    mu_run_test(test_sfs_add_directory);
    mu_run_test(test_get_root);
    mu_run_test(test_get_subdirectory);
    mu_run_test(test_get_subdirectory_level_three);
    mu_run_test(test_create_and_release_buffer);
    mu_run_test(test_add_small_data);
    mu_run_test(test_add_big_data);
    mu_run_test(test_read_partial_data);
    mu_run_test(test_read_data_at_offset);
    mu_run_test(test_read_data_over_the_end);
    mu_run_test(test_read_data_at_offset_and_over_the_end);

    return 0;
}

#endif //SPOTIFS_TESTS_H
