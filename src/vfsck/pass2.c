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

RCSID
    ("$Header: /cvs/openafs/src/vfsck/pass2.c,v 1.5.2.2 2005/10/03 02:46:33 shadow Exp $");

#define VICE
#include <sys/time.h>
#include <sys/param.h>
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
#include <ufs/fsdir.h>
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
#include <sys/dir.h>
#endif
#include <sys/fs.h>

#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_OSF_ENV */
#include <afs/osi_inode.h>

#ifdef	AFS_SUN5_ENV
#include <string.h>
#else
#include <strings.h>
#endif
#include "fsck.h"

int pass2check();

pass2()
{
    register struct dinode *dp;
    struct inodesc rootdesc;

    memset((char *)&rootdesc, 0, sizeof(struct inodesc));
    rootdesc.id_type = ADDR;
    rootdesc.id_func = pass2check;
    rootdesc.id_number = ROOTINO;
    pathp = pathname;
#if defined(ACLS) && defined(AFS_HPUX_ENV)
    switch (statemap[ROOTINO] & STATE) {
#else /* no ACLS */
    switch (statemap[ROOTINO]) {
#endif /* ACLS */

    case USTATE:
	pfatal("ROOT INODE UNALLOCATED");
	if (reply("ALLOCATE") == 0)
	    errexit("");
	if (allocdir(ROOTINO, ROOTINO, 0755) != ROOTINO)
	    errexit("CANNOT ALLOCATE ROOT INODE\n");
	descend(&rootdesc, ROOTINO);
	break;

    case DCLEAR:
	pfatal("DUPS/BAD IN ROOT INODE");
	if (reply("REALLOCATE")) {
	    freeino(ROOTINO);
	    if (allocdir(ROOTINO, ROOTINO, 0755) != ROOTINO)
		errexit("CANNOT ALLOCATE ROOT INODE\n");
	    descend(&rootdesc, ROOTINO);
	    break;
	}
	if (reply("CONTINUE") == 0)
	    errexit("");
	statemap[ROOTINO] = DSTATE;
	descend(&rootdesc, ROOTINO);
	break;

#ifdef VICE
    case VSTATE:
#endif /* VICE */
    case FSTATE:
    case FCLEAR:
	pfatal("ROOT INODE NOT DIRECTORY");
	if (reply("REALLOCATE")) {
	    freeino(ROOTINO);
	    if (allocdir(ROOTINO, ROOTINO, 0755) != ROOTINO)
		errexit("CANNOT ALLOCATE ROOT INODE\n");
	    descend(&rootdesc, ROOTINO);
	    break;
	}
	if (reply("FIX") == 0)
	    errexit("");
	dp = ginode(ROOTINO);
	dp->di_mode &= ~IFMT;
	dp->di_mode |= IFDIR;
#ifdef	AFS_SUN5_ENV
	dp->di_smode = dp->di_mode;
#endif
	inodirty();
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	/*
	 * Keep any info on associated continuation inode
	 */
	if (statemap[ROOTINO] & HASCINODE)
	    statemap[ROOTINO] = DSTATE | HASCINODE;
	else
	    statemap[ROOTINO] = DSTATE;
#else /* no ACLS */
	statemap[ROOTINO] = DSTATE;
#endif /* ACLS */
	/* fall into ... */

    case DSTATE:
	descend(&rootdesc, ROOTINO);
	break;

    default:
	errexit("BAD STATE %d FOR ROOT INODE", statemap[ROOTINO]);
    }
}

pass2check(idesc)
     struct inodesc *idesc;
{
    register struct direct *dirp = idesc->id_dirp;
    char *curpathloc;
    int n, entrysize, ret = 0;
    struct dinode *dp;
    struct direct proto;
    char namebuf[BUFSIZ];
#if defined(ACLS) && defined(AFS_HPUX_ENV)
    int holdstate;
#endif /* ACLS */

    /* 
     * check for "."
     */
    if (idesc->id_entryno != 0)
	goto chk1;
    if (dirp->d_ino != 0 && strcmp(dirp->d_name, ".") == 0) {
	if (dirp->d_ino != idesc->id_number) {
	    direrror(idesc->id_number, "BAD INODE NUMBER FOR '.'");
	    dirp->d_ino = idesc->id_number;
	    if (reply("FIX") == 1)
		ret |= ALTERED;
	}
	goto chk1;
    }
    direrror(idesc->id_number, "MISSING '.'");
    proto.d_ino = idesc->id_number;
    proto.d_namlen = 1;
    (void)strcpy(proto.d_name, ".");
    entrysize = DIRSIZ(&proto);
    if (dirp->d_ino != 0 && strcmp(dirp->d_name, "..") != 0) {
	pfatal("CANNOT FIX, FIRST ENTRY IN DIRECTORY CONTAINS %s\n",
	       dirp->d_name);
#if	defined(AFS_SUN_ENV) 
	iscorrupt = 1;
#endif
    } else if (dirp->d_reclen < entrysize) {
	pfatal("CANNOT FIX, INSUFFICIENT SPACE TO ADD '.'\n");
#if	defined(AFS_SUN_ENV) 
	iscorrupt = 1;
#endif
    } else if (dirp->d_reclen < 2 * entrysize) {
	proto.d_reclen = dirp->d_reclen;
	memcpy((char *)dirp, (char *)&proto, entrysize);
	if (reply("FIX") == 1)
	    ret |= ALTERED;
    } else {
	n = dirp->d_reclen - entrysize;
	proto.d_reclen = entrysize;
	memcpy((char *)dirp, (char *)&proto, entrysize);
	idesc->id_entryno++;
	lncntp[dirp->d_ino]--;
	dirp = (struct direct *)((char *)(dirp) + entrysize);
	memset((char *)dirp, 0, n);
	dirp->d_reclen = n;
	if (reply("FIX") == 1)
	    ret |= ALTERED;
    }
  chk1:
    if (idesc->id_entryno > 1)
	goto chk2;
    proto.d_ino = idesc->id_parent;
    proto.d_namlen = 2;
    (void)strcpy(proto.d_name, "..");
    entrysize = DIRSIZ(&proto);
    if (idesc->id_entryno == 0) {
	n = DIRSIZ(dirp);
	if (dirp->d_reclen < n + entrysize)
	    goto chk2;
	proto.d_reclen = dirp->d_reclen - n;
	dirp->d_reclen = n;
	idesc->id_entryno++;
	lncntp[dirp->d_ino]--;
	dirp = (struct direct *)((char *)(dirp) + n);
	memset((char *)dirp, 0, n);
	dirp->d_reclen = n;
    }
    if (dirp->d_ino != 0 && strcmp(dirp->d_name, "..") == 0) {
	if (dirp->d_ino != idesc->id_parent) {
	    direrror(idesc->id_number, "BAD INODE NUMBER FOR '..'");
	    dirp->d_ino = idesc->id_parent;
	    if (reply("FIX") == 1)
		ret |= ALTERED;
	}
	goto chk2;
    }
    direrror(idesc->id_number, "MISSING '..'");
    if (dirp->d_ino != 0 && strcmp(dirp->d_name, ".") != 0) {
	pfatal("CANNOT FIX, SECOND ENTRY IN DIRECTORY CONTAINS %s\n",
	       dirp->d_name);
#if	defined(AFS_SUN_ENV) 
	iscorrupt = 1;
#endif
    } else if (dirp->d_reclen < entrysize) {
	pfatal("CANNOT FIX, INSUFFICIENT SPACE TO ADD '..'\n");
#if	defined(AFS_SUN_ENV) 
	iscorrupt = 1;
#endif
    } else {
	proto.d_reclen = dirp->d_reclen;
	memcpy((char *)dirp, (char *)&proto, entrysize);
	if (reply("FIX") == 1)
	    ret |= ALTERED;
    }
  chk2:
    if (dirp->d_ino == 0)
	return (ret | KEEPON);
    if (dirp->d_namlen <= 2 && dirp->d_name[0] == '.'
	&& idesc->id_entryno >= 2) {
	if (dirp->d_namlen == 1) {
	    direrror(idesc->id_number, "EXTRA '.' ENTRY");
	    dirp->d_ino = 0;
	    if (reply("FIX") == 1)
		ret |= ALTERED;
	    return (KEEPON | ret);
	}
	if (dirp->d_name[1] == '.') {
	    direrror(idesc->id_number, "EXTRA '..' ENTRY");
	    dirp->d_ino = 0;
	    if (reply("FIX") == 1)
		ret |= ALTERED;
	    return (KEEPON | ret);
	}
    }
    curpathloc = pathp;
    *pathp++ = '/';
    if (pathp + dirp->d_namlen >= endpathname) {
	*pathp = '\0';
	errexit("NAME TOO LONG %s%s\n", pathname, dirp->d_name);
    }
    memcpy(pathp, dirp->d_name, (int)dirp->d_namlen + 1);
    pathp += dirp->d_namlen;
    idesc->id_entryno++;
    n = 0;
    if (dirp->d_ino > maxino || dirp->d_ino <= 0) {
	direrror(dirp->d_ino, "I OUT OF RANGE");
	n = reply("REMOVE");
    } else {
      again:
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	switch (statemap[dirp->d_ino] & STATE) {
#else /* no ACLS */
	switch (statemap[dirp->d_ino]) {
#endif /* ACLS */
	case USTATE:
	    direrror(dirp->d_ino, "UNALLOCATED");
	    n = reply("REMOVE");
	    break;

	case DCLEAR:
	case FCLEAR:
	    direrror(dirp->d_ino, "DUP/BAD");
	    if ((n = reply("REMOVE")) == 1)
		break;
	    dp = ginode(dirp->d_ino);
#ifdef VICE
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	    holdstate =
		(dp->di_mode & IFMT) ==
		IFDIR ? DSTATE : (VICEINODE ? VSTATE : FSTATE);
	    if (statemap[dirp->d_ino] & HASCINODE)
		statemap[dirp->d_ino] = holdstate | HASCINODE;
	    else
		statemap[dirp->d_ino] = holdstate;
#else
	    statemap[dirp->d_ino] =
		(dp->di_mode & IFMT) ==
		IFDIR ? DSTATE : (VICEINODE ? VSTATE : FSTATE);
#endif
#else /* VICE */
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	    holdstate = (dp->di_mode & IFMT) == IFDIR ? DSTATE : FSTATE;
	    if (statemap[dirp->d_ino] & HASCINODE)
		statemap[dirp->d_ino] = holdstate | HASCINODE;
	    else
		statemap[dirp->d_ino] = holdstate;
#else
	    statemap[dirp->d_ino] =
		(dp->di_mode & IFMT) == IFDIR ? DSTATE : FSTATE;
#endif
#endif /* VICE */
	    lncntp[dirp->d_ino] = dp->di_nlink;
	    goto again;

	case DFOUND:
	    if (idesc->id_entryno > 2) {
		getpathname(namebuf, dirp->d_ino, dirp->d_ino);
		pwarn("%s %s %s\n", pathname,
		      "IS AN EXTRANEOUS HARD LINK TO DIRECTORY", namebuf);
		if (preen)
		    printf(" (IGNORED)\n");
		else if ((n = reply("REMOVE")) == 1)
		    break;
	    }
	    /* fall through */

	case FSTATE:
#ifdef VICE
	  filecase:
#endif /* VICE */
	    lncntp[dirp->d_ino]--;
	    break;

#ifdef VICE
	case VSTATE:
	    direrror(dirp->d_ino, "VICE INODE REFERENCED BY DIRECTORY");
	    if (reply("CONVERT TO REGULAR FILE") != 1)
		break;
	    if ((dp = ginode(dirp->d_ino)) == NULL)
		break;
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN56_ENV)
	    dp->di_gen = dp->di_ic.ic_flags = dp->di_ic.ic_size.val[0] = 0;
#else
	    CLEAR_DVICEMAGIC(dp);
#endif
	    inodirty();
	    statemap[dirp->d_ino] = FSTATE;
	    ret |= ALTERED;
	    goto filecase;
#endif /* VICE */


	case DSTATE:
	    descend(idesc, dirp->d_ino);
	    if (statemap[dirp->d_ino] == DFOUND) {
		lncntp[dirp->d_ino]--;
	    } else if (statemap[dirp->d_ino] == DCLEAR) {
		dirp->d_ino = 0;
		ret |= ALTERED;
	    } else
		errexit("BAD RETURN STATE %d FROM DESCEND",
			statemap[dirp->d_ino]);
	    break;

#if defined(ACLS) && defined(AFS_HPUX_ENV)
	    /* hpux has more dynamic states (CSTATE, CRSTATE) */
	case CSTATE:
	    break;
	case CRSTATE:
	    break;
#endif
	default:
	    errexit("BAD STATE %d FOR INODE I=%d", statemap[dirp->d_ino],
		    dirp->d_ino);
	}
    }
    pathp = curpathloc;
    *pathp = '\0';
    if (n == 0)
	return (ret | KEEPON);
    dirp->d_ino = 0;
    return (ret | KEEPON | ALTERED);
}
