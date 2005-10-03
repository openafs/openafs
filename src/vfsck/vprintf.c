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

RCSID
    ("$Header$");

#define VICE			/* allow us to put our changes in at will */
#include <stdio.h>

#include <sys/param.h>
#include <sys/time.h>

#ifdef AFS_SUN_ENV
#define KERNEL
#endif
#include <sys/mount.h>
#ifdef AFS_SUN_ENV
#undef KERNEL
#endif /* AFS_SUN_ENV */

#include <sys/file.h>

#ifdef	AFS_OSF_ENV
#include <sys/vnode.h>
#include <sys/mount.h>
#include <ufs/inode.h>
#include <ufs/fs.h>
#define	_BSD
#define	_KERNEL
#include <ufs/dir.h>
#undef	_KERNEL
#undef	_BSD
#else /* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
#define VFS
#include <sys/vnode.h>
#ifdef	  AFS_SUN5_ENV
#include <stdio.h>
#include <unistd.h>
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
#ifdef	AFS_HPUX_ENV
#include <ctype.h>
#define	LONGFILENAMES	1
#include <sys/sysmacros.h>
#include <sys/ino.h>
#define	DIRSIZ_MACRO
#ifdef HAVE_USR_OLD_USR_INCLUDE_NDIR_H
#include </usr/old/usr/include/ndir.h>
#else
#include <ndir.h>
#endif
#else
#define KERNEL
#include <sys/dir.h>
#undef KERNEL
#endif
#include <sys/fs.h>
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_OSF_ENV */

#include <sys/stat.h>
#include <sys/wait.h>
#ifdef AFS_SUN5_ENV
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include "fsck.h"
#include <errno.h>

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
