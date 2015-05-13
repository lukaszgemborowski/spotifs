#include "support.h"
#include <string.h>

int is_path_under(const char* base, const char* path)
{
    if (strlen(path) <= strlen(base)) {
        /* if path is shorter or equal base, it can't be it's subdirectory */
        return 0;
    } else if (strncmp(base, path, strlen(base))) {
        /* base is not subset of path */
        return 0;
    } else {
        const char *p = path + strlen(base) + 1;

        /* if there is any slash character in remaining string that means
         * there is another level of directory, so it's not directly under
         * the base directory. */
        if (strchr(p, '/')) {
            return 0;
        } else {
            return 1;
        }
    }
}

char* replace_character(char *string, char in, char out)
{
    char *pos = string;

    while(*pos) {
        if (*pos == in) {
            *pos = out;
        }

        pos++;
    }

    return string;
}
