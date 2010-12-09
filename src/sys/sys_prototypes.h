/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _SYS_PROTOTYPES_H
#define _SYS_PROTOTYPES_H

/* glue.c */
#ifdef AFS_LINUX20_ENV
extern int proc_afs_syscall(long, long, long, long, long, int *);
#endif
#ifdef AFS_DARWIN80_ENV
extern int ioctl_afs_syscall(long, long, long, long, long, long, long, int *);
#endif
#ifdef AFS_SUN511_ENV
extern int ioctl_sun_afs_syscall(long, uintptr_t, uintptr_t, uintptr_t,
                                 uintptr_t, uintptr_t, uintptr_t, int*);
#endif

/* pioctl.c */
extern int lpioctl(char *, int, void *, int);

/* rmtsysc.c */
struct ViceIoctl;
extern int pioctl(char *path, afs_int32 cmd, struct ViceIoctl *data,
		  afs_int32 follow);
#ifdef AFS_DUX40_ENV
extern int afs_setpag(void);
#else
extern int setpag(void);
#endif

/* rmtsysnet.c */
extern void inparam_conversion(afs_int32, char *, afs_int32);
extern void outparam_conversion(afs_int32, char *, afs_int32);

/* rmtsyss.c */
extern void rmt_Quit(char *msg, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);

extern void rmtsysd(void);

/* setpag.c */
extern int lsetpag(void);

#endif
