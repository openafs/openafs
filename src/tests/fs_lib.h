/* Prototypes for fs_lib.c. */

#ifndef FS_LIB_H
#define FS_LIB_H 1

#include <afs/stds.h>
#include <afs/afsint.h>

struct VenusFid {
    afs_int32 Cell;
    struct AFSFid Fid;
};

int fs_getfid(char *, struct VenusFid *);
int fs_rmmount(const char *);

#endif /* !FS_LIB_H */
