/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
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
