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

int pass4check();

pass4()
{
    ino_t inumber;
    struct zlncnt *zlnp;
    struct inodesc idesc;
    int n;
#if defined(ACLS) && defined(AFS_HPUX_ENV)
    struct dinode *dp;
#endif /* ACLS */


    memset(&idesc, 0, sizeof(struct inodesc));
    idesc.id_type = ADDR;
    idesc.id_func = pass4check;
    for (inumber = ROOTINO; inumber <= lastino; inumber++) {
	idesc.id_number = inumber;
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	switch (statemap[inumber] & STATE) {
#else /* no ACLS */
	switch (statemap[inumber]) {
#endif /* ACLS */

	case FSTATE:
	case DFOUND:
	    n = lncntp[inumber];
	    if (n)
		adjust(&idesc, (short)n);
	    else {
		for (zlnp = zlnhead; zlnp; zlnp = zlnp->next)
		    if (zlnp->zlncnt == inumber) {
			zlnp->zlncnt = zlnhead->zlncnt;
			zlnp = zlnhead;
			zlnhead = zlnhead->next;
			free((char *)zlnp);
			clri(&idesc, "UNREF", 1);
			break;
		    }
	    }
	    break;

#ifdef VICE
	case VSTATE:
	    nViceFiles++;
	    break;
#endif /* VICE */

	case DSTATE:
	    clri(&idesc, "UNREF", 1);
	    break;

	case DCLEAR:
	case FCLEAR:
	    clri(&idesc, "BAD/DUP", 1);
	    break;

	case USTATE:
	    break;

#if defined(ACLS) && defined(AFS_HPUX_ENV)
	    /*
	     * UNreferenced continuation inode
	     */
	case CSTATE:
	    clri(&idesc, "UNREF", 2);
	    break;

	    /*
	     * referenced continuation inode
	     */
	case CRSTATE:
	    if ((dp = ginode(inumber)) == NULL)
		break;
	    if (dp->di_nlink != 1)
		if (!qflag) {
		    pwarn("BAD LINK COUNT IN CONTINUATION INODE ");
		    pwarn("I=%u (%ld should be %ld)", inumber, dp->di_nlink,
			  1);
		    if (preen)
#ifdef VICE
			vprintf(" (CORRECTED)\n");
#else
			printf(" (CORRECTED)\n");
#endif /* VICE */
		    else {
			if (reply("CORRECT") == 0)
			    continue;
		    }
		    dp->di_nlink = 1;
		    inodirty();
		}
	    break;
#endif /* ACLS */

	default:
	    errexit("BAD STATE %d FOR INODE I=%d", statemap[inumber],
		    inumber);
	}
    }
}

pass4check(idesc)
     struct inodesc *idesc;
{
    struct dups *dlp;
    int nfrags, res = KEEPON;
    daddr_t blkno = idesc->id_blkno;

    for (nfrags = idesc->id_numfrags; nfrags > 0; blkno++, nfrags--) {
	if (chkrange(blkno, 1)) {
	    res = SKIP;
	} else if (testbmap(blkno)) {
	    for (dlp = duplist; dlp; dlp = dlp->next) {
		if (dlp->dup != blkno)
		    continue;
		dlp->dup = duplist->dup;
		dlp = duplist;
		duplist = duplist->next;
		free((char *)dlp);
		break;
	    }
	    if (dlp == 0) {
		clrbmap(blkno);
		n_blks--;
	    }
	}
    }
    return (res);
}
