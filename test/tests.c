#include "tests.h"
#include <stdio.h>

int tests_run = 0;

int main()
{
    char *result = all_tests();

    if (result != 0) {
        printf("FAILED: %s\n", result);
    } else {
        printf("ALL TESTS PASSED\n");
    }

    printf("Tests run: %d\n", tests_run);

    return result != 0;
}