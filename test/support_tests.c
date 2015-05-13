#include "minunit.h"
#include "support.c"

char* test_is_path_under()
{
    mu_assert("is_path_under(/abc, /abc/def)",      is_path_under("/abc", "/abc/def"));
    mu_assert("is_path_under(/abc, /abc/def/ghi)", !is_path_under("/abc", "/abc/def/ghi"));
    mu_assert("is_path_under(/abc, /abcd/ef)",     !is_path_under("/abc", "/abcd/ef"));
    mu_assert("is_path_under(/abc, /def)",         !is_path_under("/abc", "/def"));
    mu_assert("is_path_under(/abc, /abc)",         !is_path_under("/abc", "/abc"));
    mu_assert("is_path_under(/, /def)",             is_path_under("/", "/def"));

    return 0;
}
