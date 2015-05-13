#include "minunit.h"
#include "sfs.c"

char* test_add_one_child()
{
    struct sfs_entry root = {0};
    struct sfs_entry node = {
            .name = "node"
    };

    sfs_add_child_entry(&root, &node);

    mu_assert("have child", root.children);
    mu_assert("have proper child", root.children == &node);

    return 0;
}

char* test_sfs_add_directory()
{
    struct sfs_entry root = {0};

    sfs_add_subdirectory(&root, "test");

    mu_assert("have child", root.children);
    mu_assert("have proper child", !strcmp("test", root.children->name));

    return 0;
}

char* test_get_root()
{
    struct sfs_entry root = {0};

    sfs_add_subdirectory(&root, "a");
    sfs_add_subdirectory(&root, "b");

    mu_assert("get root", sfs_get(&root, "/") == &root);

    return 0;
}

char* test_get_subdirectory()
{
    struct sfs_entry root = {0};

    struct sfs_entry* a = sfs_add_subdirectory(&root, "a");
    struct sfs_entry* b = sfs_add_subdirectory(&root, "b");

    mu_assert("get a", sfs_get(&root, "/a") == a);
    mu_assert("get b", sfs_get(&root, "/b") == b);
    mu_assert("get c", !sfs_get(&root, "/c"));

    return 0;
}

char* test_get_subdirectory_level_three()
{
    struct sfs_entry root = {0};

    /*
     * /-.
     *   |_ a
     *   |_ b_
     *        |_c
     *        |_d__
     *        |_e  |_f
     *             |_g
     */

    struct sfs_entry* a = sfs_add_subdirectory(&root, "a");
    struct sfs_entry* b = sfs_add_subdirectory(&root, "b");

    struct sfs_entry* c = sfs_add_subdirectory(b, "c");
    struct sfs_entry* d = sfs_add_subdirectory(b, "d");
    struct sfs_entry* e = sfs_add_subdirectory(b, "e");

    struct sfs_entry* f = sfs_add_subdirectory(d, "f");
    struct sfs_entry* g = sfs_add_subdirectory(d, "g");


    mu_assert("get a", sfs_get(&root, "/a") == a);
    mu_assert("get b", sfs_get(&root, "/b") == b);
    mu_assert("get c", sfs_get(&root, "/b/c") == c);
    mu_assert("get d", sfs_get(&root, "/b/d") == d);
    mu_assert("get e", sfs_get(&root, "/b/e") == e);
    mu_assert("get f", sfs_get(&root, "/b/d/f") == f);
    mu_assert("get g", sfs_get(&root, "/b/d/g") == g);

    return 0;
}