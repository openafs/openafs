/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
#ifndef __FS_UTILS_H_ENV__
#define __FS_UTILS_H_ENV__ 1


/* pioctl opcode constants */
#include "smb_iocons.h"

/* PRSFS_ constants */
#include <afs/prs_fs.h>
#include <afs/pioctl_nt.h>

#include <winsock2.h>

/* Fake error code since NT errno.h doesn't define it */
#include <afs/errmap_nt.h>

extern char *hostutil_GetNameByINet(long addr);

extern struct hostent *hostutil_GetHostByName(char *namep);

extern long util_GetInt32(char *stringp, long *valuep);

extern long fs_StripDriveLetter(char *inPathp, char *outPathp, long outSize);

extern long fs_ExtractDriveLetter(char *inPathp, char *outPathp);

#endif /* FS_UTILS_H_ENV */
