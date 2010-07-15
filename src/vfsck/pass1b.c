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
#include <stdio.h>
#undef	_KERNEL
#undef	_BSD
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

#include "fsck.h"

int pass1bcheck();
static struct dups *duphead;

pass1b()
{
    int c, i;
    struct dinode *dp;
    struct inodesc idesc;
    ino_t inumber;

    memset(&idesc, 0, sizeof(struct inodesc));
    idesc.id_type = ADDR;
    idesc.id_func = pass1bcheck;
    duphead = duplist;
    inumber = 0;
    for (c = 0; c < sblock.fs_ncg; c++) {
	for (i = 0; i < sblock.fs_ipg; i++, inumber++) {
	    if (inumber < ROOTINO)
		continue;
	    dp = ginode(inumber);
	    if (dp == NULL)
		continue;
	    idesc.id_number = inumber;
#ifdef	AFS_SUN5_ENV
	    idesc.id_fix = DONTKNOW;
#endif
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	    if (((statemap[inumber] & STATE) != USTATE) &&
#else /* not ACLS */
	    if (statemap[inumber] != USTATE &&
#endif /* ACLS */
		(ckinode(dp, &idesc) & STOP))
		return;
	}
    }
}

pass1bcheck(idesc)
     struct inodesc *idesc;
{
    struct dups *dlp;
    int nfrags, res = KEEPON;
    daddr_t blkno = idesc->id_blkno;

    for (nfrags = idesc->id_numfrags; nfrags > 0; blkno++, nfrags--) {
	if (chkrange(blkno, 1))
	    res = SKIP;
	for (dlp = duphead; dlp; dlp = dlp->next) {
	    if (dlp->dup == blkno) {
		blkerror(idesc->id_number, "DUP", blkno);
		dlp->dup = duphead->dup;
		duphead->dup = blkno;
		duphead = duphead->next;
	    }
	    if (dlp == muldup)
		break;
	}
	if (muldup == 0 || duphead == muldup->next)
	    return (STOP);
    }
    return (res);
}
