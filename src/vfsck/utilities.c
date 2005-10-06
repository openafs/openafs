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
    ("$Header: /cvs/openafs/src/vfsck/utilities.c,v 1.5.2.3 2005/10/03 02:46:33 shadow Exp $");

#include <sys/param.h>
#define VICE			/* allow us to put our changes in at will */
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
#define	AFS_NEWCG_ENV
#else /* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
#include <sys/vnode.h>
#ifdef	  AFS_SUN5_ENV
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
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
#include <sys/signal.h>
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

#include <stdio.h>
#include <ctype.h>
#include "fsck.h"

#if	defined(AFS_HPUX101_ENV)
#include <unistd.h>
#endif

#if	defined(AFS_SUN_ENV) || defined(AFS_OSF_ENV)
#define AFS_NEWCG_ENV
#else
#undef AFS_NEWCG_ENV
#endif

long diskreads, totalreads;	/* Disk cache statistics */
#if	!defined(AFS_HPUX101_ENV)
long lseek();
#endif
char *malloc();
#if	defined(AFS_SUN_ENV)
extern int iscorrupt;
#endif
#ifdef	AFS_SUN_ENV
extern int isdirty;
#endif
#ifdef	AFS_SUN5_ENV
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/stat.h>
#include <sys/vfstab.h>

offset_t llseek();
#endif

ftypeok(dp)
     struct dinode *dp;
{
    switch (dp->di_mode & IFMT) {

    case IFDIR:
    case IFREG:
    case IFBLK:
    case IFCHR:
    case IFLNK:
    case IFSOCK:
#ifdef	AFS_HPUX_ENV
    case IFNWK:
#endif
#ifdef IFIFO
    case IFIFO:
#else
#ifdef	IFPORT
    case IFPORT:
#endif
#endif /* IFIFO */
	return (1);

    default:
	if (debug)
	    printf("bad file type 0%o\n", dp->di_mode);
	return (0);
    }
}

#ifdef	AFS_HPUX_ENV
extern int fixed;
#endif
reply(question)
     char *question;
{
    int persevere;
    char c;

    if (preen)
	pfatal("INTERNAL ERROR: GOT TO reply()");
    persevere = !strcmp(question, "CONTINUE");
    printf("\n");
    if (!persevere && (nflag || fswritefd < 0)) {
	printf("%s? no\n\n", question);
#if	defined(AFS_SUN_ENV) 
	iscorrupt = 1;		/* known to be corrupt */
#endif
	return (0);
    }
    if (yflag || (persevere && nflag)) {
	printf("%s? yes\n\n", question);
	return (1);
    }
    do {
	printf("%s? [yn] ", question);
	(void)fflush(stdout);
	c = getc(stdin);
	while (c != '\n' && getc(stdin) != '\n')
	    if (feof(stdin))
		return (0);
    } while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');
    printf("\n");
    if (c == 'y' || c == 'Y')
	return (1);
#ifdef	AFS_HPUX_ENV
    fixed = 0;
#endif
#if	defined(AFS_SUN_ENV)
    iscorrupt = 1;		/* known to be corrupt */
#endif
    return (0);
}

/*
 * Malloc buffers and set up cache.
 */
bufinit()
{
    register struct bufarea *bp;
    long bufcnt, i;
    char *bufp;

    bufp = malloc((unsigned int)sblock.fs_bsize);
    if (bufp == 0)
	errexit("cannot allocate buffer pool\n");
    cgblk.b_un.b_buf = bufp;
    initbarea(&cgblk);
    bufhead.b_next = bufhead.b_prev = &bufhead;
    bufcnt = MAXBUFSPACE / sblock.fs_bsize;
    if (bufcnt < MINBUFS)
	bufcnt = MINBUFS;
    for (i = 0; i < bufcnt; i++) {
	bp = (struct bufarea *)malloc(sizeof(struct bufarea));
	bufp = malloc((unsigned int)sblock.fs_bsize);
	if (bp == NULL || bufp == NULL) {
	    if (i >= MINBUFS)
		break;
	    errexit("cannot allocate buffer pool\n");
	}
	bp->b_un.b_buf = bufp;
	bp->b_prev = &bufhead;
	bp->b_next = bufhead.b_next;
	bufhead.b_next->b_prev = bp;
	bufhead.b_next = bp;
	initbarea(bp);
    }
    bufhead.b_size = i;		/* save number of buffers */
#ifdef	AFS_SUN5_ENVX
    pbp = pdirbp = NULL;
#endif
}

/*
 * Manage a cache of directory blocks.
 */
struct bufarea *
getdatablk(blkno, size)
     daddr_t blkno;
     long size;
{
    register struct bufarea *bp;

    for (bp = bufhead.b_next; bp != &bufhead; bp = bp->b_next)
	if (bp->b_bno == fsbtodb(&sblock, blkno))
	    goto foundit;
    for (bp = bufhead.b_prev; bp != &bufhead; bp = bp->b_prev)
	if ((bp->b_flags & B_INUSE) == 0)
	    break;
    if (bp == &bufhead)
	errexit("deadlocked buffer pool\n");
    getblk(bp, blkno, size);
    /* fall through */
  foundit:
    totalreads++;
    bp->b_prev->b_next = bp->b_next;
    bp->b_next->b_prev = bp->b_prev;
    bp->b_prev = &bufhead;
    bp->b_next = bufhead.b_next;
    bufhead.b_next->b_prev = bp;
    bufhead.b_next = bp;
    bp->b_flags |= B_INUSE;
    return (bp);
}

struct bufarea *
getblk(bp, blk, size)
     register struct bufarea *bp;
     daddr_t blk;
     long size;
{
    daddr_t dblk;

    dblk = fsbtodb(&sblock, blk);
    if (bp->b_bno == dblk)
	return (bp);
    flush(fswritefd, bp);
    diskreads++;
    bp->b_errs = bread(fsreadfd, bp->b_un.b_buf, dblk, size);
    bp->b_bno = dblk;
    bp->b_size = size;
    return (bp);
}

flush(fd, bp)
     int fd;
     register struct bufarea *bp;
{
    register int i, j;
    caddr_t sip;
    long size;

    if (!bp->b_dirty)
	return;
    if (bp->b_errs != 0) {
	pfatal("WRITING %sZERO'ED BLOCK %d TO DISK\n",
	       (bp->b_errs == bp->b_size / dev_bsize) ? "" : "PARTIALLY ",
	       bp->b_bno);
    }
    bp->b_dirty = 0;
    bp->b_errs = 0;
    bwrite(fd, bp->b_un.b_buf, bp->b_bno, (long)bp->b_size);
    if (bp != &sblk)
	return;
#if	defined(AFS_HPUX101_ENV)
#if     defined(AFS_HPUX110_ENV)
    /* jpm: Need a fix here */
    if (0)
#else /* AFS_HPUX110_ENV */
    if (bwrite
	(fswritefd, (char *)sblock.fs_csp[0],
	 fsbtodb(&sblock, sblock.fs_csaddr), fragroundup(&sblock,
							 sblock.fs_cssize)) ==
	1)
#endif /* else AFS_HPUX110_ENV */
    {
	printf
	    ("\nUnable to write to cylinder group summary area (fs_csaddr)\n");
	printf("\tDisk write error at block %u\n",
	       fsbtodb(&sblock, sblock.fs_csaddr));
    }
#else
#if	defined(AFS_SUN56_ENV)
    sip = (caddr_t) sblock.fs_u.fs_csp;
    for (i = 0, j = 0; i < sblock.fs_cssize; i += sblock.fs_bsize, j++) {
	size =
	    sblock.fs_cssize - i <
	    sblock.fs_bsize ? sblock.fs_cssize - i : sblock.fs_bsize;
	bwrite(fswritefd, sip,
	       fsbtodb(&sblock, sblock.fs_csaddr + j * sblock.fs_frag), size);
	sip += size;
    }
#else
    for (i = 0, j = 0; i < sblock.fs_cssize; i += sblock.fs_bsize, j++) {
	bwrite(fswritefd, (char *)sblock.fs_csp[j],
	       fsbtodb(&sblock, sblock.fs_csaddr + j * sblock.fs_frag),
	       sblock.fs_cssize - i <
	       sblock.fs_bsize ? sblock.fs_cssize - i : sblock.fs_bsize);
    }
#endif /* AFS_SUN56_ENV */
#endif /* AFS_HPUX101_ENV */
}

rwerror(mesg, blk)
     char *mesg;
     daddr_t blk;
{

    if (preen == 0)
	printf("\n");
    pfatal("CANNOT %s: BLK %ld", mesg, blk);
    if (reply("CONTINUE") == 0)
	errexit("Program terminated\n");
}

ckfini()
{
    register struct bufarea *bp, *nbp;
    int cnt = 0;

    flush(fswritefd, &sblk);
#ifdef AFS_NEWCG_ENV
    if (havesb && sblk.b_bno != SBOFF / dev_bsize && !preen
	&& reply("UPDATE STANDARD SUPERBLOCK")) {
	sblk.b_bno = SBOFF / dev_bsize;
#else /* AFS_NEWCG_ENV */
    if (havesb && sblk.b_bno != SBLOCK && !preen
	&& reply("UPDATE STANDARD SUPERBLOCK")) {
	sblk.b_bno = SBLOCK;
#endif /* AFS_NEWCG_ENV */
	sbdirty();
	flush(fswritefd, &sblk);
    }
    flush(fswritefd, &cgblk);
    if (cgblk.b_un.b_buf) {
	free(cgblk.b_un.b_buf);
	cgblk.b_un.b_buf = NULL;
    }
    for (bp = bufhead.b_prev; bp != &bufhead; bp = nbp) {
	cnt++;
	flush(fswritefd, bp);
	nbp = bp->b_prev;
	free(bp->b_un.b_buf);
	free((char *)bp);
    }
#ifdef	AFS_SUN5_ENVX
    pbp = pdirbp = NULL;
#endif
    if (bufhead.b_size != cnt)
	errexit("Panic: lost %d buffers\n", bufhead.b_size - cnt);
    if (debug)
	printf("cache missed %d of %d (%d%%)\n", diskreads, totalreads,
	       diskreads * 100 / totalreads);
    (void)close(fsreadfd);
    (void)close(fswritefd);
}

#if	!defined(AFS_HPUX101_ENV)
bread(fd, buf, blk, size)
     int fd;
     char *buf;
#ifdef AFS_SUN59_ENV
     diskaddr_t blk;
#else
     daddr_t blk;
#endif
     long size;
{
    char *cp;
    int i, errs;
#ifdef	AFS_SUN5_ENV
    offset_t offset = (offset_t) blk << DEV_BSHIFT;
    offset_t addr;
#endif

#ifdef	AFS_HPUX_ENV
    /* To fix a stripping related bug?? */
    if (lseek(fd, blk * dev_bsize, 0) == -1)
#else
#ifdef	AFS_SUN5_ENV
    if (llseek(fd, offset, 0) < 0)
#else
    if (lseek(fd, blk * dev_bsize, 0) < 0)
#endif
#endif
	rwerror("SEEK", blk);
    else if (read(fd, buf, (int)size) == size)
	return (0);
    rwerror("READ", blk);
#ifdef	AFS_HPUX_ENV
    /* To fix a stripping related bug?? */
    if (lseek(fd, blk * dev_bsize, 0) == -1)
#else
#ifdef	AFS_SUN5_ENV
    if (llseek(fd, offset, 0) < 0)
#else
    if (lseek(fd, blk * dev_bsize, 0) < 0)
#endif
#endif
	rwerror("SEEK", blk);
    errs = 0;
    memset(buf, 0, (int)size);
    printf("THE FOLLOWING DISK SECTORS COULD NOT BE READ:");
#ifdef	AFS_SUN5_ENV
    for (cp = buf, i = 0; i < btodb(size); i++, cp += DEV_BSIZE) {
	addr = (offset_t) (blk + i) << DEV_BSHIFT;
	if (llseek(fd, addr, SEEK_CUR) < 0 || read(fd, cp, (int)secsize) < 0) {
	    printf(" %d", blk + i);
#else
    for (cp = buf, i = 0; i < size; i += secsize, cp += secsize) {
	if (read(fd, cp, (int)secsize) < 0) {
	    lseek(fd, blk * dev_bsize + i + secsize, 0);
	    if (secsize != dev_bsize && dev_bsize != 1)
		printf(" %d (%d),", (blk * dev_bsize + i) / secsize,
		       blk + i / dev_bsize);
	    else
		printf(" %d,", blk + i / dev_bsize);
#endif
	    errs++;
	}
    }
    printf("\n");
    return (errs);
}

bwrite(fd, buf, blk, size)
     int fd;
     char *buf;
#ifdef AFS_SUN59_ENV
     diskaddr_t blk;
#else
     daddr_t blk;
#endif
     long size;
{
    int i, n;
    char *cp;
#ifdef	AFS_SUN5_ENV
    offset_t offset = (offset_t) blk << DEV_BSHIFT;
    offset_t addr;

    if (blk < SBLOCK) {
	char msg[256];
	sprintf(msg, "WARNING: Attempt to write illegal blkno %d on %s\n",
		blk, devname);
	if (debug)
	    printf(msg);
	return;
    }
#endif

    if (fd < 0)
	return;
#ifdef	AFS_HPUX_ENV
    /* To fix a stripping related bug?? */
    if (lseek(fd, blk * dev_bsize, 0) == -1)
#else
#ifdef	AFS_SUN5_ENV
    if (llseek(fd, offset, 0) < 0)
#else
    if (lseek(fd, blk * dev_bsize, 0) < 0)
#endif
#endif
	rwerror("SEEK", blk);
    else if (write(fd, buf, (int)size) == size) {
	fsmodified = 1;
	return;
    }
    rwerror("WRITE", blk);
#ifdef	AFS_HPUX_ENV
    /* To fix a stripping related bug?? */
    if (lseek(fd, blk * dev_bsize, 0) == -1)
#else
#ifdef	AFS_SUN5_ENV
    if (llseek(fd, offset, 0) < 0)
#else
    if (lseek(fd, blk * dev_bsize, 0) < 0)
#endif
#endif
	rwerror("SEEK", blk);
    printf("THE FOLLOWING SECTORS COULD NOT BE WRITTEN:");
#ifdef	AFS_SUN5_ENV
    for (cp = buf, i = 0; i < btodb(size); i++, cp += DEV_BSIZE) {
	n = 0;
	addr = (offset_t) (blk + i) << DEV_BSHIFT;
	if (llseek(fd, addr, SEEK_CUR) < 0
	    || (n = write(fd, cp, DEV_BSIZE)) < 0) {
	    printf(" %d", blk + i);
	} else if (n > 0) {
	    fsmodified = 1;
	}

    }
#else
    for (cp = buf, i = 0; i < size; i += dev_bsize, cp += dev_bsize)
	if (write(fd, cp, (int)dev_bsize) < 0) {
	    lseek(fd, blk * dev_bsize + i + dev_bsize, 0);
	    printf(" %d,", blk + i / dev_bsize);
	}
#endif
    printf("\n");
    return;
}
#endif /* AFS_HPUX101_ENV */
/*
 * allocate a data block with the specified number of fragments
 */
allocblk(frags)
     long frags;
{
    register int i, j, k;

    if (frags <= 0 || frags > sblock.fs_frag)
	return (0);
    for (i = 0; i < maxfsblock - sblock.fs_frag; i += sblock.fs_frag) {
	for (j = 0; j <= sblock.fs_frag - frags; j++) {
	    if (testbmap(i + j))
		continue;
	    for (k = 1; k < frags; k++)
		if (testbmap(i + j + k))
		    break;
	    if (k < frags) {
		j += k;
		continue;
	    }
	    for (k = 0; k < frags; k++)
		setbmap(i + j + k);
	    n_blks += frags;
	    return (i + j);
	}
    }
    return (0);
}

/*
 * Free a previously allocated block
 */
freeblk(blkno, frags)
     daddr_t blkno;
     long frags;
{
    struct inodesc idesc;

    idesc.id_blkno = blkno;
    idesc.id_numfrags = frags;
    pass4check(&idesc);
}

/*
 * Find a pathname
 */
getpathname(namebuf, curdir, ino)
     char *namebuf;
     ino_t curdir, ino;
{
    int len;
    register char *cp;
    struct inodesc idesc;
    extern int findname();

    if (statemap[ino] != DSTATE && statemap[ino] != DFOUND) {
	strcpy(namebuf, "?");
	return;
    }
    memset((char *)&idesc, 0, sizeof(struct inodesc));
    idesc.id_type = DATA;
    cp = &namebuf[BUFSIZ - 1];
    *cp = '\0';
    if (curdir != ino) {
	idesc.id_parent = curdir;
	goto namelookup;
    }
    while (ino != ROOTINO) {
	idesc.id_number = ino;
	idesc.id_func = findino;
	idesc.id_name = "..";
#ifdef	AFS_SUN5_ENV
	idesc.id_fix = NOFIX;
#endif
	if ((ckinode(ginode(ino), &idesc) & FOUND) == 0)
	    break;
      namelookup:
	idesc.id_number = idesc.id_parent;
	idesc.id_parent = ino;
	idesc.id_func = findname;
	idesc.id_name = namebuf;
#ifdef	AFS_SUN5_ENV
	idesc.id_fix = NOFIX;
#endif
	if ((ckinode(ginode(idesc.id_number), &idesc) & FOUND) == 0)
	    break;
	len = strlen(namebuf);
	cp -= len;
	if (cp < &namebuf[MAXNAMLEN])
	    break;
	memcpy(cp, namebuf, len);
	*--cp = '/';
	ino = idesc.id_number;
    }
    if (ino != ROOTINO) {
	strcpy(namebuf, "?");
	return;
    }
    memcpy(namebuf, cp, &namebuf[BUFSIZ] - cp);
}

void
catch()
{
    ckfini();
#ifdef	AFS_SUN5_ENV
    exit(37);
#else
    exit(12);
#endif
}

/*
 * When preening, allow a single quit to signal
 * a special exit after filesystem checks complete
 * so that reboot sequence may be interrupted.
 */
void
catchquit()
{
    extern returntosingle;

    printf("returning to single-user after filesystem check\n");
    returntosingle = 1;
    (void)signal(SIGQUIT, SIG_DFL);
}

/*
 * Ignore a single quit signal; wait and flush just in case.
 * Used by child processes in preen.
 */
void
voidquit()
{

    sleep(1);
    (void)signal(SIGQUIT, SIG_IGN);
    (void)signal(SIGQUIT, SIG_DFL);
}

/*
 * determine whether an inode should be fixed.
 */
dofix(idesc, msg)
     register struct inodesc *idesc;
     char *msg;
{

    switch (idesc->id_fix) {

    case DONTKNOW:
	if (idesc->id_type == DATA)
	    direrror(idesc->id_number, msg);
	else
	    pwarn(msg);
	if (preen) {
	    printf(" (SALVAGED)\n");
	    idesc->id_fix = FIX;
	    return (ALTERED);
	}
	if (reply("SALVAGE") == 0) {
	    idesc->id_fix = NOFIX;
	    return (0);
	}
	idesc->id_fix = FIX;
	return (ALTERED);

    case FIX:
	return (ALTERED);

    case NOFIX:
	return (0);

    default:
	errexit("UNKNOWN INODESC FIX MODE %d\n", idesc->id_fix);
    }
    /* NOTREACHED */
}

/* VARARGS1 */
errexit(s1, s2, s3, s4)
     char *s1;
     long s2, s3, s4;
{
    printf(s1, s2, s3, s4);
#ifdef	AFS_SUN5_ENV
    exit(39);
#else
    exit(8);
#endif
}

/*
 * An unexpected inconsistency occured.
 * Die if preening, otherwise just print message and continue.
 */
/* VARARGS1 */
pfatal(s, a1, a2, a3)
     char *s;
     long a1, a2, a3;
{
    if (preen) {
	printf("%s: ", devname);
	printf(s, a1, a2, a3);
	printf("\n");
	printf("%s: UNEXPECTED INCONSISTENCY; RUN fsck MANUALLY.\n", devname);
#ifdef	AFS_SUN5_ENV
	exit(36);
#else
	exit(8);
#endif
    }
    printf(s, a1, a2, a3);
}

/*
 * Pwarn just prints a message when not preening,
 * or a warning (preceded by filename) when preening.
 */
/* VARARGS1 */
pwarn(s, a1, a2, a3, a4, a5, a6)
     char *s;
     long a1, a2, a3, a4, a5, a6;
{
    if (preen)
	printf("%s: ", devname);
    printf(s, a1, a2, a3, a4, a5, a6);
}

/*
 * Pwarn just prints a message when not preening,
 * or a warning (preceded by filename) when preening.
 */
/* VARARGS1 */
pinfo(s, a1, a2, a3, a4, a5, a6)
     char *s;
     long a1, a2, a3, a4, a5, a6;
{
    if (preen)
	printf("%s: ", devname);
    printf(s, a1, a2, a3, a4, a5, a6);
}

#ifndef lint
/*
 * Stub for routines from kernel.
 */
panic(s)
     char *s;
{

    pfatal("INTERNAL INCONSISTENCY:");
    errexit(s);
}
#endif

#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN3_ENV)
debugclean()
{
    char s[256];

    if (debug == 0)
	return;
    if ((iscorrupt == 0) && (isdirty == 0))
	return;
    if ((sblock.fs_clean != FSSTABLE) && (sblock.fs_clean != FSCLEAN))
	return;
#ifdef	AFS_SUN5_ENV
    if (FSOKAY != (sblock.fs_state + sblock.fs_time))
#else
    if (FSOKAY != (fs_get_state(&sblock) + sblock.fs_time))
#endif
	return;

    sprintf(s, "WARNING: inconsistencies detected on `%s' filesystem %s",
	    sblock.fs_clean == FSSTABLE ? "stable" : "clean", devname);
    printf("%s\n", s);
}

updateclean()
{
    struct bufarea cleanbuf;
    unsigned int size;
    unsigned int bno;
    unsigned int fsclean;
#if	defined(AFS_SUN56_ENV)
    offset_t sblkoff;
#endif

    debugclean();
    /* set fsclean to its appropriate value */
    fsclean = sblock.fs_clean;
#ifdef	AFS_SUN5_ENV
    if (FSOKAY != (sblock.fs_state + sblock.fs_time))
#else
    if (FSOKAY != (fs_get_state(&sblock) + sblock.fs_time))
#endif
	fsclean = FSACTIVE;

    /* if necessary, update fs_clean and fs_state */
    switch (fsclean) {
    case FSACTIVE:
	if (iscorrupt == 0)
	    fsclean = FSSTABLE;
	break;
    case FSCLEAN:
    case FSSTABLE:
	if (iscorrupt)
	    fsclean = FSACTIVE;
	break;
    default:
	if (iscorrupt)
	    fsclean = FSACTIVE;
	else
	    fsclean = FSSTABLE;
    }
    /* if fs_clean and fs_state are ok, do nothing */
    if ((sblock.fs_clean == fsclean) &&
#ifdef	AFS_SUN5_ENV
	(FSOKAY == (sblock.fs_state + sblock.fs_time)))
#else
	(FSOKAY == (fs_get_state(&sblock) + sblock.fs_time)))
#endif
	return;
    sblock.fs_clean = fsclean;
#ifdef	AFS_SUN5_ENV
    sblock.fs_state = sblock.fs_time;
#else
    fs_set_state(&sblock, sblock.fs_time);
#endif
    /* if superblock can't be written, return */
    if (fswritefd < 0)
	return;
    /* read private copy of superblock, update clean flag, and write it */
    bno = sblk.b_bno;
    size = sblk.b_size;
#if	defined(AFS_SUN56_ENV)
    sblkoff = (OFF_T) (bno) << DEV_BSHIFT;
    if (llseek(fsreadfd, sblkoff, 0) == -1)
	return;
#else
    if (lseek(fsreadfd, (off_t) dbtob(bno), 0) == -1)
	return;
#endif
    if ((cleanbuf.b_un.b_buf = malloc(size)) == NULL)
	errexit("out of memory");
    if (read(fsreadfd, cleanbuf.b_un.b_buf, (int)size) != size)
	return;
    cleanbuf.b_un.b_fs->fs_clean = sblock.fs_clean;
#ifdef	AFS_SUN5_ENV
    cleanbuf.b_un.b_fs->fs_state = sblock.fs_state;
#else
    fs_set_state(cleanbuf.b_un.b_fs, fs_get_state(&sblock));
#endif
    cleanbuf.b_un.b_fs->fs_time = sblock.fs_time;
#if	defined(AFS_SUN56_ENV)
    if (llseek(fswritefd, sblkoff, 0) == -1)
	return;
#else
    if (lseek(fswritefd, (off_t) dbtob(bno), 0) == -1)
	return;
#endif
    if (write(fswritefd, cleanbuf.b_un.b_buf, (int)size) != size)
	return;
}

printclean()
{
    char *s;

#ifdef	AFS_SUN5_ENV
    if (FSOKAY != (sblock.fs_state + sblock.fs_time))
#else
    if (FSOKAY != (fs_get_state(&sblock) + sblock.fs_time))
#endif
	s = "unknown";
    else

	switch (sblock.fs_clean) {
	case FSACTIVE:
	    s = "active";
	    break;
	case FSCLEAN:
	    s = "clean";
	    break;
	case FSSTABLE:
	    s = "stable";
	    break;
	default:
	    s = "unknown";
	}
    if (preen)
	pwarn("is %s.\n", s);
    else
	printf("** %s is %s.\n", devname, s);
}
#endif

#ifdef	AFS_SUN52_ENV
char *
hasvfsopt(vfs, opt)
     register struct vfstab *vfs;
     register char *opt;
{
    char *f, *opts;
    static char *tmpopts;

    if (vfs->vfs_mntopts == NULL)
	return (NULL);
    if (tmpopts == 0) {
	tmpopts = (char *)calloc(256, sizeof(char));
	if (tmpopts == 0)
	    return (0);
    }
    strncpy(tmpopts, vfs->vfs_mntopts, (sizeof(tmpopts) - 1));
    opts = tmpopts;
    f = mntopt(&opts);
    for (; *f; f = mntopt(&opts)) {
	if (strncmp(opt, f, strlen(opt)) == 0)
	    return (f - tmpopts + vfs->vfs_mntopts);
    }
    return (NULL);
}


writable(name)
     char *name;
{
    int rw = 1;
    struct vfstab vfsbuf;
    FILE *vfstab;
    char *blkname, *unrawname();

    vfstab = fopen(VFSTAB, "r");
    if (vfstab == NULL) {
	printf("can't open %s\n", VFSTAB);
	return (1);
    }
    blkname = unrawname(name);
    if ((getvfsspec(vfstab, &vfsbuf, blkname) == 0)
	&& (vfsbuf.vfs_fstype != NULL)
	&& (strcmp(vfsbuf.vfs_fstype, MNTTYPE_UFS) == 0)
	&& (hasvfsopt(&vfsbuf, MNTOPT_RO))) {
	rw = 0;
    }
    fclose(vfstab);
    return (rw);
}

mounted(name)
     char *name;
{
    int found = 0;
    struct mnttab mnt;
    FILE *mnttab;
    struct stat device_stat, mount_stat;
    char *blkname, *unrawname();

    mnttab = fopen(MNTTAB, "r");
    if (mnttab == NULL) {
	printf("can't open %s\n", MNTTAB);
	return (0);
    }
    blkname = unrawname(name);
    while ((getmntent(mnttab, &mnt)) == NULL) {
	if (strcmp(mnt.mnt_fstype, MNTTYPE_UFS) != 0) {
	    continue;
	}
	if (strcmp(blkname, mnt.mnt_special) == 0) {
	    stat(mnt.mnt_mountp, &mount_stat);
	    stat(mnt.mnt_special, &device_stat);
	    if (device_stat.st_rdev == mount_stat.st_dev) {
		if (hasmntopt(&mnt, MNTOPT_RO) != 0)
		    found = 2;	/* mounted as RO */
		else
		    found = 1;	/* mounted as R/W */
	    }
	    break;
	}
    }
    fclose(mnttab);
    return (found);
}

#endif

#if	defined(AFS_HPUX101_ENV)

#include "libfs.h"
#include <sys/stat.h>
#include <sys/fcntl.h>

int seek_options;

bread(fd, buf, blk, size)
     int fd;
     char *buf;
     daddr_t blk;
     long size;
{
    int i = 0;
    off_t lseek_offset;

    switch (seek_options) {
    case BLKSEEK_ENABLED:
	lseek_offset = blk;
	break;

    case BLKSEEK_NOT_ENABLED:	/* File */
#if	defined(AFS_HPUX102_ENV)
	lseek_offset = dbtoo(blk);
#else
	lseek_offset = dbtob(blk);
#endif
	break;

    default:
	rwerror("BLKSEEK", blk);
    }
    if (lseek(fd, (off_t) lseek_offset, 0) == (off_t) - 1)
	rwerror("SEEK", blk);
    else if (read(fd, buf, (int)size) == size)
	return 0;
    rwerror("READ", blk);
    return 1;
}

bwrite(fd, buf, blk, size)
     int fd;
     char *buf;
     daddr_t blk;
     long size;
{
    off_t lseek_offset;

    if (fd < 0)
	return 1;

    switch (seek_options) {
    case BLKSEEK_ENABLED:
	lseek_offset = blk;
	break;

    case BLKSEEK_NOT_ENABLED:	/* File */
#if	defined(AFS_HPUX102_ENV)
	lseek_offset = dbtoo(blk);
#else
	lseek_offset = dbtob(blk);
#endif
	break;

    default:
	rwerror("BLKSEEK", blk);
    }
    if (lseek(fd, (off_t) lseek_offset, 0) == (off_t) - 1)
	rwerror("SEEK", blk);
    else if (write(fd, buf, (int)size) == size) {
	fsmodified = 1;
	return 0;
    }
    rwerror("WRITE", blk);
    return 1;
}

int
setup_block_seek(fd)
     int fd;			/* File descriptor */
{
    int flags = 0;		/* Flags to/from fcntl() */

    return setup_block_seek_2(fd, &flags);
}

#define set_fserror printf

int
setup_block_seek_2(fd, cntl_flags)
     int fd;			/* Input.     File descriptor    */
     int *cntl_flags;		/* Returned.  Flags from fcntl() */
{
    int flags = 0;		/* Flags to/from fcntl() */
    off_t lstatus;		/* Status from lseek()   */
    struct stat statarea;
    char dummy[MAXBSIZE];

    *cntl_flags = 0;
    /*
     *  Get control Flags
     */
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
	set_fserror("Cannot get file control flags.");
	return (BLKSEEK_PROCESSING_ERROR);
    }

    /*
     *  Check if fd is a non-device file
     */

    if (fstat(fd, &statarea) == -1) {
	set_fserror("Cannot get status information on file descriptor.");
	return (BLKSEEK_PROCESSING_ERROR);
    }
    if (((statarea.st_mode & S_IFMT) != S_IFCHR)
	&& ((statarea.st_mode & S_IFMT) != S_IFBLK)) {
	/* Not a device file -- BLKSEEK only works on device files */
	*cntl_flags = flags;	/* O_BLKSEEK bit never set */
	return (BLKSEEK_NOT_ENABLED);
    }

    /*
     *  Set the fd to O_BLKSEEK
     */
    if (fcntl(fd, F_SETFL, flags | O_BLKSEEK) == -1) {
	set_fserror("Cannot set file control flags.");
	return (BLKSEEK_PROCESSING_ERROR);
    }
    if (flags & O_WRONLY) {
	/* Set lseek to location 0 before returning */
	if (lseek(fd, (off_t) 0, SEEK_SET) == (off_t) - 1) {
	    set_fserror("Cannot lseek the device.");
	    return (BLKSEEK_PROCESSING_ERROR);
	}
	if (debug)
	    set_fserror
		("File is write only.  Cannot Verify O_BLKSEEK mode set.");
	*cntl_flags = flags | O_BLKSEEK;
	return (BLKSEEK_FILE_WRITEONLY);
    }

    /*
     *  Verify that kernel knows how to do O_BLKSEEK
     */
    if (lseek(fd, (off_t) 0, SEEK_SET) == (off_t) - 1) {
	set_fserror("Cannot lseek the device.");
	return (BLKSEEK_PROCESSING_ERROR);
    }
    if (read(fd, &dummy[0], BBSIZE) != BBSIZE) {	/* BBSIZE = 8192 */
	set_fserror("Cannot read from the device");
	return (BLKSEEK_PROCESSING_ERROR);
    }

    if ((lstatus = lseek(fd, (off_t) 0, SEEK_CUR)) == (off_t) - 1) {
	set_fserror("Cannot lseek the device.");
	return (BLKSEEK_PROCESSING_ERROR);
    }

    /*
     *  Set lseek to location 0
     */
    if (lseek(fd, (off_t) 0, SEEK_SET) == (off_t) - 1) {
	set_fserror("Cannot lseek the device.");
	return (BLKSEEK_PROCESSING_ERROR);
    }

    if (lstatus == (off_t) (BBSIZE / DEV_BSIZE)) {
	*cntl_flags = flags | O_BLKSEEK;
	return (BLKSEEK_ENABLED);	/* Successfully enabled O_BLKSEEK */
    }
    *cntl_flags = flags & (~O_BLKSEEK);	/* Turn off O_BLKSEEK bit */
    return (BLKSEEK_NOT_ENABLED);	/* O_BLKSEEK not enabled */
}

setup_all_block_seek()
{
    int opt = 0;
    seek_options = setup_block_seek(fsreadfd);
    switch (seek_options) {
    case BLKSEEK_ENABLED:
    case BLKSEEK_NOT_ENABLED:
	break;
    default:
	errexit("setup_block_seek on fsreadfd");
	break;
    }
    if (fswritefd == -1)
	goto done;
    switch (opt = setup_block_seek(fswritefd)) {
    case BLKSEEK_FILE_WRITEONLY:	/* WO block or char device. */
    case BLKSEEK_NOT_ENABLED:	/* regular file.            */
	if (seek_options != opt)
	    printf
		("seek_options on fsreadfd (%d) and fswritefd (%d) do not match",
		 seek_options, opt);
	break;
    default:
	errexit("setup_block_seek on fswritefd");
	break;
    }
  done:
    if (debug)
	printf("read option = %d write option = %d\n", seek_options, opt);
}



#endif /* AFS_HPUX101_ENV */
