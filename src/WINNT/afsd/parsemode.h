/*
 * Copyright 1991 by Vincent Archer.
 *    You may freely redistribute this software, in source or binary
 *    form, provided that you do not alter this copyright mention in any
 *    way.
 */

#ifndef OPENAFS_WINNT_AFSD_PARSEMODE_H
#define OPENAFS_WINNT_AFSD_PARSEMODE_H
extern afs_uint32 parsemode(char *symbolic, afs_uint32 oldmode);

#define USR_MODES (S_ISUID|S_IRWXU)
#define GRP_MODES (S_ISGID|S_IRWXG)
#define EXE_MODES (S_IXUSR|S_IXGRP|S_IXOTH)
#ifdef S_ISVTX
#define ALL_MODES (USR_MODES|GRP_MODES|S_IRWXO|S_ISVTX)
#else
#define ALL_MODES (USR_MODES|GRP_MODES|S_IRWXO)
#endif

#endif /* OPENAFS_WINNT_AFSD_PARSEMODE_H */
