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

#ifndef _MFC_VER
#include <winsock2.h>
#endif

/* Fake error code since NT errno.h doesn't define it */
#include <afs/errmap_nt.h>

#ifndef hostutil_GetNameByINet
extern char *hostutil_GetNameByINet(afs_uint32 addr);
#endif

#ifndef hostutil_GetHostByName
extern struct hostent *hostutil_GetHostByName(char *namep);
#endif

extern long util_GetInt32(char *stringp, long *valuep);

extern long fs_StripDriveLetter(char *inPathp, char *outPathp, long outSize);

extern long fs_ExtractDriveLetter(char *inPathp, char *outPathp);

extern char *cm_mount_root;             /*"afs"*/
extern char *cm_slash_mount_root;       /*"/afs"*/
extern char *cm_back_slash_mount_root;  /*"\\afs"*/
extern void fs_utils_InitMountRoot();
#endif /* FS_UTILS_H_ENV */
