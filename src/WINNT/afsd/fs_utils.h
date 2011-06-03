/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_FS_UTILS_H
#define OPENAFS_WINNT_AFSD_FS_UTILS_H 1


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

extern afs_int32 util_GetInt32(char *stringp, afs_int32 *valuep);

extern void fs_utils_InitMountRoot();

extern long fs_StripDriveLetter(char *inPathp, char *outPathp, size_t outSize);

extern long fs_ExtractDriveLetter(char *inPathp, char *outPathp);

extern long fs_GetFullPath(char *pathp, char *outPathp, size_t outSize);

extern char *fs_GetParent(char *apath);

extern int fs_InAFS(char *apath);

extern int fs_IsFreelanceRoot(char *apath);

#define NETBIOSNAMESZ 1024
extern const char * fs_NetbiosName(void);

extern BOOL fs_IsAdmin (void);

extern char **fs_MakeUtf8Cmdline(int argc, const wchar_t **wargv);

extern void fs_FreeUtf8CmdLine(int argc, char ** argv);

extern void fs_Die(int code, char *filename);

extern const char * fs_filetypestr(afs_uint32 type);

extern void fs_SetProcessName(const char *name);

extern char *cm_mount_root;             /*"afs"*/
extern char *cm_slash_mount_root;       /*"/afs"*/
extern char *cm_back_slash_mount_root;  /*"\\afs"*/
extern void fs_utils_InitMountRoot();
#endif /* OPENAFS_WINNT_AFSD_FS_UTILS_H */
