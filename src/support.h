#ifndef SPOTIFS_SUPPORT_H
#define SPOTIFS_SUPPORT_H

/*
 * check if (full) path is a directory directly under base, eg:
 * /a/b is in /a
 * /a/b/c is not in /a
 */
int is_path_under(const char* base, const char* path);
char* replace_character(char *string, char in, char out);

#endif //SPOTIFS_SUPPORT_H
