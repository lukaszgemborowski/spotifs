#ifndef SPOTIFS_FS_H
#define SPOTIFS_FS_H

#define FUSE_USE_VERSION 30
#include <fuse.h>

extern struct fuse_operations spotifs_operations;

#endif // SPOTIFS_FS_H
