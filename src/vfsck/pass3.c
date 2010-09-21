/*
 * Copyright (c) 1980, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <afsconfig.h>
#include <afs/param.h>


#define VICE
#include <sys/param.h>
#include <sys/time.h>

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
#include <stdio.h>
#else /* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
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
#endif
#else /* AFS_VFSINCL_ENV */
#include <sys/inode.h>
#ifdef	AFS_HPUX_ENV
#include <ctype.h>
#define	LONGFILENAMES	1
#include <sys/sysmacros.h>
#include <sys/ino.h>
#endif
#include <sys/fs.h>
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_OSF_ENV */

#include <afs/osi_inode.h>
#include "fsck.h"

int pass2check();

pass3()
{
    struct dinode *dp;
    struct inodesc idesc;
    ino_t inumber, orphan;
    int loopcnt;

    memset(&idesc, 0, sizeof(struct inodesc));
    idesc.id_type = DATA;
    for (inumber = ROOTINO; inumber <= lastino; inumber++) {
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	if (statemap[inumber] & HASCINODE) {
	    if ((dp = ginode(inumber)) == NULL)
		break;
	    /*
	     * Make sure di_contin is not out of range and then
	     * check and make sure that the inode #di_contin
	     * is a continuation inode that has not already been
	     * referenced.
	     */
	    if ((dp->di_contin < ROOTINO || dp->di_contin > maxino)
		|| ((statemap[dp->di_contin] & STATE) != CSTATE)) {
		/*  this is an error which must be cleared by hand. */
		pfatal("BAD CONTINUATION INODE NUMBER ");
#ifdef VICE
		vprintf(" I=%u ", inumber);
#else
		printf(" I=%u ", inumber);
#endif /* VICE */
		if (reply("CLEAR") == 1) {
		    dp->di_contin = 0;
		    inodirty();
		}
	    } else {
		statemap[dp->di_contin] = CRSTATE;
	    }
	}
	if ((statemap[inumber] & STATE) == DSTATE) {
#else /* no ACLS */
	if (statemap[inumber] == DSTATE) {
#endif /* ACLS */
	    pathp = pathname;
	    *pathp++ = '?';
	    *pathp = '\0';
	    idesc.id_func = findino;
	    idesc.id_name = "..";
	    idesc.id_parent = inumber;
	    loopcnt = 0;
	    do {
		orphan = idesc.id_parent;
		if (orphan < ROOTINO || orphan > maxino)
		    break;
		dp = ginode(orphan);
		idesc.id_parent = 0;
		idesc.id_number = orphan;
		if ((ckinode(dp, &idesc) & FOUND) == 0)
		    break;
		if (loopcnt >= sblock.fs_cstotal.cs_ndir)
		    break;
		loopcnt++;
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	    } while ((statemap[idesc.id_parent] & STATE) == DSTATE);
#else /* no ACLS */
	    } while (statemap[idesc.id_parent] == DSTATE);
#endif /* ACLS */
	    if (linkup(orphan, idesc.id_parent) == 1) {
		idesc.id_func = pass2check;
		idesc.id_number = lfdir;
		descend(&idesc, orphan);
	    }
	}
    }
}
