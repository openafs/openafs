/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* I don't really need all thes, but I can't tell which ones I need
 * and which I don't.
 */
#include <afsconfig.h>
#include <afs/param.h>

#ifdef AFS_HPUX_ENV
/* We need the old directory type headers (included below), so don't include
 * the normal dirent.h, or it will conflict. */
# undef HAVE_DIRENT_H
# include <sys/inode.h>
# define	LONGFILENAMES	1
# include <sys/sysmacros.h>
# include <sys/ino.h>
# define	DIRSIZ_MACRO
# ifdef HAVE_USR_OLD_USR_INCLUDE_NDIR_H
#  include </usr/old/usr/include/ndir.h>
# else
#  include <ndir.h>
# endif
#endif

#include <roken.h>

#include <ctype.h>

#define VICE			/* allow us to put our changes in at will */

#ifdef AFS_SUN_ENV
#define KERNEL
#endif
#include <sys/mount.h>
#ifdef AFS_SUN_ENV
#undef KERNEL
#endif /* AFS_SUN_ENV */

#include <sys/file.h>

#ifdef AFS_VFSINCL_ENV
#define VFS
#include <sys/vnode.h>
#ifdef	  AFS_SUN5_ENV
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fs.h>
#define _KERNEL
#include <sys/fs/ufs_fsdir.h>
#undef _KERNEL
#include <sys/fs/ufs_mount.h>
#else
#include <ufs/inode.h>
#include <ufs/fs.h>
#define KERNEL
#include <ufs/fsdir.h>
#undef KERNEL
#endif
#else /* AFS_VFSINCL_ENV */

#include <sys/inode.h>
#ifndef	AFS_HPUX_ENV
#define KERNEL
#include <sys/dir.h>
#undef KERNEL
#endif
#include <sys/fs.h>
#endif /* AFS_VFSINCL_ENV */

#include <sys/wait.h>
#include "fsck.h"

/* Vice printf:  lob the message into /vice/file/vfsck.log if it isn't the root */
/* If we're salvaging the root, it wouldn't be such a good idea to lob stuff there */
vfscklogprintf(s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
     char *s;
     long a1, a2, a3, a4, a5, a6, a7, a8, a9, a10;
{
    printf(s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
    if (logfile) {
	fprintf(logfile, s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	fflush(logfile);
    }
}
