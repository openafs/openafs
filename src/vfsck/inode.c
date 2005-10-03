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
    ("$Header$");

#define VICE			/* control whether AFS changes are present */
#include <stdio.h>

#include <sys/time.h>
#include <sys/param.h>

#ifdef	AFS_OSF_ENV
#include <sys/mount.h>
#include <sys/vnode.h>
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
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fs.h>
#define _KERNEL
#include <sys/fs/ufs_fsdir.h>
#undef _KERNEL
#else
#include <ufs/inode.h>
#define KERNEL
#include <ufs/fsdir.h>
#undef	KERNEL
#include <ufs/fs.h>
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
#include <pwd.h>
#include "fsck.h"

#ifdef AFS_SUN_ENV
#ifdef	AFS_SUN5_ENV
#include <sys/mnttab.h>
#include <sys/mntent.h>
#else
#include <mntent.h>
#endif
#endif /* AFS_SUN_ENV */

struct bufarea *pbp = 0;

ckinode(dp, idesc)
     struct dinode *dp;
     register struct inodesc *idesc;
{
    register daddr_t *ap;
    long ret, n, ndb, offset;
    struct dinode dino;
    UOFF_T indir_data_blks;
    extern int pass1check();

#ifdef	AFS_HPUX_ENV
    if (FASTLNK)
	return (KEEPON);
#endif
    idesc->id_fix = DONTKNOW;
    idesc->id_entryno = 0;
    idesc->id_filesize = dp->di_size;
    if ((dp->di_mode & IFMT) == IFBLK || (dp->di_mode & IFMT) == IFCHR)
	return (KEEPON);
#ifdef	AFS_OSF_ENV
    if ((dp->di_flags & IC_FASTLINK) != 0) {
	return (KEEPON);
    }
#endif /* AFS_OSF_ENV */
    dino = *dp;
    ndb = howmany(dino.di_size, (UOFF_T) sblock.fs_bsize);
    ap = &dino.di_db[0];
#ifdef AFS_OSF_ENV
    /*
     * Check property lists on pass1
     */
    if (idesc->id_func == pass1check && dino.di_flags & IC_PROPLIST && *ap) {
	ret = proplist_scan(dp, idesc);
	if (ret & STOP)
	    return (ret);
    }
#endif /* AFS_OSF_ENV */
    for (; ap < &dino.di_db[NDADDR]; ap++) {
	if (--ndb == 0 && (offset = blkoff(&sblock, dino.di_size)) != 0)
	    idesc->id_numfrags =
		numfrags(&sblock, fragroundup(&sblock, offset));
	else
	    idesc->id_numfrags = sblock.fs_frag;
	if (*ap == 0)
	    continue;
	idesc->id_blkno = *ap;
	if (idesc->id_type == ADDR)
	    ret = (*idesc->id_func) (idesc);
	else
	    ret = dirscan(idesc);
	if (ret & STOP)
	    return (ret);
    }
    idesc->id_numfrags = sblock.fs_frag;
#if	defined(AFS_SUN56_ENV)
    /*
     * indir_data_blks determine the no. of data blocks
     * in the previous levels. ie., at level 3 it
     * is the number of data blocks at level 2, 1, and 0.
     */
    for (ap = &dino.di_ib[0], n = 1; n <= NIADDR; ap++, n++) {
	if (n == 1) {
	    /* SINGLE */
	    indir_data_blks = NDADDR;
	} else if (n == 2) {
	    /* DOUBLE */
	    indir_data_blks = NDADDR + NINDIR(&sblock);
	} else if (n == 3) {
	    /* TRIPLE */
	    indir_data_blks =
		NDADDR + NINDIR(&sblock) +
		(NINDIR(&sblock) * NINDIR(&sblock));
	}
	if (*ap) {
	    idesc->id_blkno = *ap;
	    ret =
		iblock(idesc, n,
		       (u_offset_t) howmany(dino.di_size,
					    (u_offset_t) sblock.fs_bsize) -
		       indir_data_blks);
	    if (ret & STOP)
		return (ret);
	}
    }
#else
    for (ap = &dino.di_ib[0], n = 1; n <= NIADDR; ap++, n++) {
	if (*ap) {
	    idesc->id_blkno = *ap;
	    ret = iblock(idesc, n, dino.di_size - sblock.fs_bsize * NDADDR);
	    if (ret & STOP)
		return (ret);
	}
    }
#endif
    return (KEEPON);
}

iblock(idesc, ilevel, isize)
     struct inodesc *idesc;
     register long ilevel;
     UOFF_T isize;
{
    register daddr_t *ap;
    register daddr_t *aplim;
    int i, n, (*func) ();
    UOFF_T sizepb;
    OFF_T nif;
    register struct bufarea *bp;
    char buf[BUFSIZ];
    extern int dirscan(), pass1check();

    if (idesc->id_type == ADDR) {
	func = idesc->id_func;
	if (((n = (*func) (idesc)) & KEEPON) == 0)
	    return (n);
    } else
	func = dirscan;
    if (chkrange(idesc->id_blkno, idesc->id_numfrags))
	return (SKIP);
    bp = getdatablk(idesc->id_blkno, sblock.fs_bsize);
    ilevel--;
#if	defined(AFS_SUN56_ENV)
    for (sizepb = 1, i = 0; i < ilevel; i++) {
	sizepb *= (u_offset_t) NINDIR(&sblock);
    }

    /*
     * nif indicates the next "free" pointer (as an array index) in this
     * indirect block, based on counting the blocks remaining in the
     * file after subtracting all previously processed blocks.
     * This figure is based on the size field of the inode.
     *
     * Note that in normal operation, nif may initially calculated to
     * be larger than the number of pointers in this block; if that is
     * the case, nif is limited to the max number of pointers per
     * indirect block.
     *
     * Also note that if an inode is inconsistant (has more blocks
     * allocated to it than the size field would indicate), the sweep
     * through any indirect blocks directly pointed at by the inode
     * continues. Since the block offset of any data blocks referenced
     * by these indirect blocks is greater than the size of the file,
     * the index nif may be computed as a negative value.
     * In this case, we reset nif to indicate that all pointers in
     * this retrieval block should be zeroed and the resulting
     * unreferenced data and/or retrieval blocks be recovered
     * through garbage collection later.
     */
    nif = (offset_t) howmany(isize, sizepb);
    if (nif > NINDIR(&sblock))
	nif = NINDIR(&sblock);
    else if (nif < 0)
	nif = 0;
#else
    for (sizepb = sblock.fs_bsize, i = 0; i < ilevel; i++)
	sizepb *= (UOFF_T) NINDIR(&sblock);
    nif = isize / sizepb + 1;
    if (nif > NINDIR(&sblock))
	nif = NINDIR(&sblock);
#endif
    if (idesc->id_func == pass1check && nif < NINDIR(&sblock)) {
	aplim = &bp->b_un.b_indir[NINDIR(&sblock)];
	for (ap = &bp->b_un.b_indir[nif]; ap < aplim; ap++) {
	    if (*ap == 0)
		continue;
	    (void)sprintf(buf, "PARTIALLY TRUNCATED INODE I=%d",
			  idesc->id_number);
	    if (dofix(idesc, buf)) {
		*ap = 0;
		dirty(bp);
	    }
	}
	flush(fswritefd, bp);
    }
    aplim = &bp->b_un.b_indir[nif];
    for (ap = bp->b_un.b_indir, i = 1; ap < aplim; ap++, i++) {
	if (*ap) {
	    idesc->id_blkno = *ap;
	    if (ilevel > 0) {
#if	defined(AFS_SUN56_ENV)
		n = iblock(idesc, ilevel, isize);
		/*
		 * each iteration decrease "remaining block
		 * count" by however many blocks were accessible
		 * by a pointer at this indirect block level.
		 */
		isize -= sizepb;
#else
		n = iblock(idesc, ilevel, isize - i * sizepb);
#endif
	    } else
		n = (*func) (idesc);
	    if (n & STOP) {
		bp->b_flags &= ~B_INUSE;
		return (n);
	    }
	}
    }
    bp->b_flags &= ~B_INUSE;
    return (KEEPON);
}

/*
 * Check that a block in a legal block number.
 * Return 0 if in range, 1 if out of range.
 */
chkrange(blk, cnt)
     daddr_t blk;
     int cnt;
{
    register int c;

    if ((unsigned)(blk + cnt) > maxfsblock)
	return (1);
    c = dtog(&sblock, blk);
    if (blk < cgdmin(&sblock, c)) {
	if ((blk + cnt) > cgsblock(&sblock, c)) {
	    if (debug) {
		printf("blk %d < cgdmin %d;", blk, cgdmin(&sblock, c));
		printf(" blk + cnt %d > cgsbase %d\n", blk + cnt,
		       cgsblock(&sblock, c));
	    }
	    return (1);
	}
    } else {
	if ((blk + cnt) > cgbase(&sblock, c + 1)) {
	    if (debug) {
		printf("blk %d >= cgdmin %d;", blk, cgdmin(&sblock, c));
		printf(" blk + cnt %d > sblock.fs_fpg %d\n", blk + cnt,
		       sblock.fs_fpg);
	    }
	    return (1);
	}
    }
    return (0);
}

struct dinode *
ginode(inumber)
     ino_t inumber;
{
    daddr_t iblk;

    if (inumber < ROOTINO || inumber > maxino)
	errexit("bad inode number %d to ginode\n", inumber);
    if (mlk_startinum == 0 || inumber < mlk_startinum
	|| inumber >= mlk_startinum + INOPB(&sblock)) {
	iblk = itod(&sblock, inumber);
	if (pbp != 0)
	    pbp->b_flags &= ~B_INUSE;
	pbp = getdatablk(iblk, sblock.fs_bsize);
	mlk_startinum = (inumber / INOPB(&sblock)) * INOPB(&sblock);
    }
    return (&pbp->b_un.b_dinode[inumber % INOPB(&sblock)]);
}

#ifdef	AFS_SUN5_ENVX
inocleanup()
{
    register struct inoinfo **inpp;

    if (inphead == NULL)
	return;
    for (inpp = &inpsort[inplast - 1]; inpp >= inpsort; inpp--)
	free((char *)(*inpp));
    free((char *)inphead);
    free((char *)inpsort);
    inphead = inpsort = NULL;
}
#endif

inodirty()
{

    dirty(pbp);
}

clri(idesc, type, flag)
     register struct inodesc *idesc;
     char *type;
     int flag;
{
    register struct dinode *dp;
#if defined(ACLS) && defined(AFS_HPUX_ENV)
    struct inodesc cidesc;
#endif /* ACLS */

    dp = ginode(idesc->id_number);
    if (flag == 1) {
	pwarn("%s %s", type, (dp->di_mode & IFMT) == IFDIR ? "DIR" : "FILE");
	pinode(idesc->id_number);
#if defined(ACLS) && defined(AFS_HPUX_ENV)
    } else if (flag == 2) {
	pwarn("%s %s", type, "CONTINUATION INODE ");
	printf(" I=%u ", idesc->id_number);
#endif /* ACLS */
    }
    if (preen || reply("CLEAR") == 1) {
	if (preen)
	    printf(" (CLEARED)\n");
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	if (CONT)
	    n_cont--;
	else
	    n_files--;

	/*
	 * If there is a CI associated with this inode, we must 
	 * clear it as well.  
	 */
	if (statemap[idesc->id_number] & HASCINODE) {
	    if (!(dp->di_contin < ROOTINO || dp->di_contin > maxino))
		cidesc.id_number = dp->di_contin;
	    clri(&cidesc, "UNREF", 2);
	}
#else /* no ACLS */
	n_files--;
#endif /* ACLS */
	(void)ckinode(dp, idesc);
#ifdef	VICE
	zapino(dp);
#else /* VICE */
	clearinode(dp);
#endif /* VICE */
	statemap[idesc->id_number] = USTATE;
	inodirty();
    }
}

findname(idesc)
     struct inodesc *idesc;
{
    register struct direct *dirp = idesc->id_dirp;

    if (dirp->d_ino != idesc->id_parent)
	return (KEEPON);
    memcpy(idesc->id_name, dirp->d_name, (int)dirp->d_namlen + 1);
    return (STOP | FOUND);
}

findino(idesc)
     struct inodesc *idesc;
{
    register struct direct *dirp = idesc->id_dirp;

    if (dirp->d_ino == 0)
	return (KEEPON);
    if (strcmp(dirp->d_name, idesc->id_name) == 0 && dirp->d_ino >= ROOTINO
	&& dirp->d_ino <= maxino) {
	idesc->id_parent = dirp->d_ino;
	return (STOP | FOUND);
    }
    return (KEEPON);
}

pinode(ino)
     ino_t ino;
{
    register struct dinode *dp;
    register char *p;
    struct passwd *pw;
    char *ctime();

    printf(" I=%u ", ino);
    if (ino < ROOTINO || ino > maxino)
	return;
    dp = ginode(ino);
    printf(" OWNER=");
#if     defined(AFS_HPUX110_ENV)
    {
	char uidbuf[BUFSIZ];
	char *p;
	uid_t uid;
	if (dp == NULL) {
	    pwarn("Could not getinode(%d) in pinode.", ino);
	    return;
	}
	uid = _GET_D_UID(dp);
	if (getpw(uid, uidbuf) == 0) {
	    for (p = uidbuf; *p != ':'; p++);
	    *p = 0;
	    printf("%s ", uidbuf);
	} else {
	    printf("%d ", uid);
	}
    }
#else /* AFS_HPUX110_ENV */
#if	defined(AFS_HPUX102_ENV)
    if ((pw = getpwuid(_GET_D_UID(dp))) != 0)
	printf("%s ", pw->pw_name);
    else
	printf("%d ", _GET_D_UID(dp));
#else /* AFS_HPUX102_ENV */
    if ((pw = getpwuid((int)dp->di_uid)) != 0)
	printf("%s ", pw->pw_name);
    else
	printf("%d ", dp->di_uid);
#endif /* else AFS_HPUX102_ENV */
#endif /* else AFS_HPUX110_ENV */
    printf("MODE=%o\n", dp->di_mode);
    if (preen)
	printf("%s: ", devname);
#if	defined(AFS_SUN56_ENV)
    printf("SIZE=%lld ", dp->di_size);
#else
    printf("SIZE=%ld ", dp->di_size);
#endif
    p = ctime(&dp->di_mtime);
    printf("MTIME=%12.12s %4.4s ", p + 4, p + 20);
}

blkerror(ino, type, blk)
     ino_t ino;
     char *type;
     daddr_t blk;
{

    pfatal("%ld %s I=%u", blk, type, ino);
    printf("\n");
    switch (statemap[ino]) {

#ifdef VICE
    case VSTATE:
#endif /* VICE */
    case FSTATE:
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	/* 
	 *  Keep the continuation inode info
	 */
	if (statemap[ino] & HASCINODE)
	    statemap[ino] = FCLEAR | HASCINODE;
	else
	    statemap[ino] = FCLEAR;
#else /* no ACLS */
	statemap[ino] = FCLEAR;
#endif /* ACLS */
	return;

    case DSTATE:
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	/* 
	 *  Keep the continuation inode info
	 */
	if (statemap[ino] & HASCINODE)
	    statemap[ino] = DCLEAR | HASCINODE;
	else
	    statemap[ino] = DCLEAR;
#else /* no ACLS */
	statemap[ino] = DCLEAR;
#endif /* ACLS */
	return;

    case FCLEAR:
    case DCLEAR:
	return;

    default:
	errexit("BAD STATE %d TO BLKERR", statemap[ino]);
	/* NOTREACHED */
    }
}

/*
 * allocate an unused inode
 */
ino_t
allocino(request, type)
     ino_t request;
     int type;
{
    register ino_t ino;
    register struct dinode *dp;

    if (request == 0)
	request = ROOTINO;
    else if (statemap[request] != USTATE)
	return (0);
    for (ino = request; ino < maxino; ino++)
	if (statemap[ino] == USTATE)
	    break;
    if (ino == maxino)
	return (0);
    switch (type & IFMT) {
    case IFDIR:
	statemap[ino] = DSTATE;
	break;
    case IFREG:
    case IFLNK:
	statemap[ino] = FSTATE;
	break;
    default:
	return (0);
    }
    dp = ginode(ino);
    dp->di_db[0] = allocblk((long)1);
    if (dp->di_db[0] == 0) {
	statemap[ino] = USTATE;
	return (0);
    }
    dp->di_mode = type;
    time(&dp->di_atime);
    dp->di_mtime = dp->di_ctime = dp->di_atime;
    dp->di_size = sblock.fs_fsize;
    dp->di_blocks = btodb(sblock.fs_fsize);

    n_files++;
    inodirty();
    return (ino);
}

/*
 * deallocate an inode
 */
freeino(ino)
     ino_t ino;
{
    struct inodesc idesc;
    extern int pass4check();
    struct dinode *dp;

    memset((char *)&idesc, 0, sizeof(struct inodesc));
    idesc.id_type = ADDR;
    idesc.id_func = pass4check;
    idesc.id_number = ino;
    dp = ginode(ino);
    (void)ckinode(dp, &idesc);
#ifdef VICE
    zapino(dp);
#else /* VICE */
    clearinode(dp);
#endif /* VICE */
    inodirty();
    statemap[ino] = USTATE;
    n_files--;
}
