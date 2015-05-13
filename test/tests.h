#ifndef SPOTIFS_TESTS_H
#define SPOTIFS_TESTS_H

#include "minunit.h"

char* test_is_path_under();

char* test_add_one_child();
char* test_sfs_add_directory();
char* test_get_root();
char* test_get_subdirectory();
char* test_get_subdirectory_level_three();

char* all_tests()
{
    mu_run_test(test_is_path_under);
    mu_run_test(test_add_one_child);
    mu_run_test(test_sfs_add_directory);
    mu_run_test(test_get_root);
    mu_run_test(test_get_subdirectory);
    mu_run_test(test_get_subdirectory_level_three);

    return 0;
}

#endif //SPOTIFS_TESTS_H
