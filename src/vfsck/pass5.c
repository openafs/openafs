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

#if	defined(AFS_SUN_ENV) || defined(AFS_OSF_ENV)
#define AFS_NEWCG_ENV
#else
#undef AFS_NEWCG_ENV
#endif

#ifndef AFS_NEWCG_ENV
/* define enough so this thing compiles! */
#define FS_42POSTBLFMT		1
#define FS_DYNAMICPOSTBLFMT	2
#endif /* AFS_NEWCG_ENV */

pass5()
{
    int c, blk, frags, basesize, sumsize, mapsize, savednrpos;
    struct fs *fs = &sblock;
    struct cg *cg = &cgrp;
    daddr_t dbase, dmax;
    daddr_t d;
    long i, j;
    struct csum *cs;
    time_t now;
    struct csum cstotal;
    struct inodesc idesc[3];
    char buf[MAXBSIZE];
    int postype;

#ifdef AFS_NEWCG_ENV
    struct cg *newcg = (struct cg *)buf;
    struct ocg *ocg = (struct ocg *)buf;
#else /* AFS_NEWCG_ENV */
    /* don't bother with newcg format yet, most systems don't support it */
    struct cg *newcg = (struct cg *)buf;
    struct cg *ocg = (struct cg *)buf;
#endif /* AFS_NEWCG_ENV */

    memset(newcg, 0, (int)fs->fs_cgsize);
    newcg->cg_niblk = fs->fs_ipg;
#ifdef AFS_NEWCG_ENV
    postype = (int)fs->fs_postblformat;
#else /* AFS_NEWCG_ENV */
    postype = FS_42POSTBLFMT;
#endif /* AFS_NEWCG_ENV */
    switch (postype) {

    case FS_42POSTBLFMT:
	basesize = (char *)(&ocg->cg_btot[0]) - (char *)(&ocg->cg_link);
	sumsize = &ocg->cg_iused[0] - (char *)(&ocg->cg_btot[0]);
	mapsize =
	    &ocg->cg_free[howmany(fs->fs_fpg, NBBY)] -
	    (u_char *) & ocg->cg_iused[0];
	ocg->cg_magic = CG_MAGIC;
#ifdef AFS_NEWCG_ENV
	savednrpos = fs->fs_nrpos;
	fs->fs_nrpos = 8;
#endif /* AFS_NEWCG_ENV */
	break;

#ifdef AFS_NEWCG_ENV
    case FS_DYNAMICPOSTBLFMT:
	newcg->cg_btotoff =
#ifdef	__alpha
	    /* Matches decl in ufs/fs.h */
	    &newcg->cg_space[0] - (u_char *) (&newcg->cg_link[0]);
#else /* __alpha */
	    &newcg->cg_space[0] - (u_char *) (&newcg->cg_link);
#endif
	newcg->cg_boff = newcg->cg_btotoff + fs->fs_cpg * sizeof(afs_int32);
	newcg->cg_iusedoff =
	    newcg->cg_boff + fs->fs_cpg * fs->fs_nrpos * sizeof(short);
	newcg->cg_freeoff = newcg->cg_iusedoff + howmany(fs->fs_ipg, NBBY);
	newcg->cg_nextfreeoff =
	    newcg->cg_freeoff + howmany(fs->fs_cpg * fs->fs_spc / NSPF(fs),
					NBBY);
	newcg->cg_magic = CG_MAGIC;
#ifdef	__alpha
	/* Matches decl in ufs/fs.h */
	basesize = &newcg->cg_space[0] - (u_char *) (&newcg->cg_link[0]);
#else /* __alpha */
	basesize = &newcg->cg_space[0] - (u_char *) (&newcg->cg_link);
#endif
	sumsize = newcg->cg_iusedoff - newcg->cg_btotoff;
	mapsize = newcg->cg_nextfreeoff - newcg->cg_iusedoff;
	break;

    default:
	errexit("UNKNOWN ROTATIONAL TABLE FORMAT %d\n", fs->fs_postblformat);
#endif /* AFS_NEWCG_ENV */

    }
    memset(&idesc[0], 0, sizeof idesc);
    for (i = 0; i < 3; i++)
	idesc[i].id_type = ADDR;
    memset(&cstotal, 0, sizeof(struct csum));
    (void)time(&now);
#ifdef notdef
    /* this is the original from UCB/McKusick, but it is clearly wrong.  It is
     * rounding the # of fragments to the next 1024 (in our case, with a 1K/8K file system),
     * while instead it should be rounding to the next block.
     *
     * In addition, we should be sure that we allocate enough space, but that seems to be
     * ensured by the fact that the bitmap is rounded up to the nearest short, and that there
     * are never more than 16 frags per block.
     */
    for (i = fs->fs_size; i < fragroundup(fs, fs->fs_size); i++)
#else
    c = 1 << fs->fs_fragshift;	/* unit to which we want to round */
    for (i = fs->fs_size; i < roundup(fs->fs_size, c); i++)
#endif
	setbmap(i);
    for (c = 0; c < fs->fs_ncg; c++) {
	getblk(&cgblk, cgtod(fs, c), fs->fs_cgsize);
#ifdef AFS_NEWCG_ENV
	if (!cg_chkmagic(cg))
	    pfatal("CG %d: BAD MAGIC NUMBER\n", c);
#else /* AFS_NEWCG_ENV */
	if (cg->cg_magic != CG_MAGIC)
	    pfatal("CG %d: BAD MAGIC NUMBER\n", c);
#endif /* AFS_NEWCG_ENV */
	dbase = cgbase(fs, c);
	dmax = dbase + fs->fs_fpg;
	if (dmax > fs->fs_size)
	    dmax = fs->fs_size;
	if (now > cg->cg_time)
	    newcg->cg_time = cg->cg_time;
	else
	    newcg->cg_time = now;
	newcg->cg_cgx = c;
	if (c == fs->fs_ncg - 1)
	    newcg->cg_ncyl = fs->fs_ncyl % fs->fs_cpg;
	else
	    newcg->cg_ncyl = fs->fs_cpg;
	newcg->cg_ndblk = dmax - dbase;
	newcg->cg_cs.cs_ndir = 0;
	newcg->cg_cs.cs_nffree = 0;
	newcg->cg_cs.cs_nbfree = 0;
	newcg->cg_cs.cs_nifree = fs->fs_ipg;
	if (cg->cg_rotor < newcg->cg_ndblk)
	    newcg->cg_rotor = cg->cg_rotor;
	else
	    newcg->cg_rotor = 0;
	if (cg->cg_frotor < newcg->cg_ndblk)
	    newcg->cg_frotor = cg->cg_frotor;
	else
	    newcg->cg_frotor = 0;
	if (cg->cg_irotor < newcg->cg_niblk)
	    newcg->cg_irotor = cg->cg_irotor;
	else
	    newcg->cg_irotor = 0;
	memset(&newcg->cg_frsum[0], 0, sizeof newcg->cg_frsum);
#ifdef AFS_NEWCG_ENV
	memset(&cg_blktot(newcg)[0], 0, sumsize + mapsize);
#else /* AFS_NEWCG_ENV */
	memset(newcg->cg_btot, 0, sizeof(newcg->cg_btot));
	memset(newcg->cg_b, 0, sizeof(newcg->cg_b));
	memset(newcg->cg_iused, 0, sizeof(newcg->cg_iused));
	memset(newcg->cg_free, 0, howmany(fs->fs_fpg, NBBY));
#endif /* AFS_NEWCG_ENV */
#ifdef AFS_NEWCG_ENV
	if (fs->fs_postblformat == FS_42POSTBLFMT)
	    ocg->cg_magic = CG_MAGIC;
#endif /* AFS_NEWCG_ENV */
	j = fs->fs_ipg * c;
	for (i = 0; i < fs->fs_ipg; j++, i++) {
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	    switch (statemap[j] & STATE) {
#else
	    switch (statemap[j]) {
#endif
	    case USTATE:
		break;

	    case DSTATE:
	    case DCLEAR:
	    case DFOUND:
		newcg->cg_cs.cs_ndir++;
		/* fall through */

#ifdef VICE
	    case VSTATE:
#endif /* VICE */
	    case FSTATE:
	    case FCLEAR:
		newcg->cg_cs.cs_nifree--;
#ifdef AFS_NEWCG_ENV
		setbit(cg_inosused(newcg), i);
#else
		setbit(newcg->cg_iused, i);
#endif /* AFS_NEWCG_ENV */
		break;

#if defined(ACLS) && defined(AFS_HPUX_ENV)
		/* hpux has more dynamic states (CSTATE, CRSTATE) */
	    case CSTATE:
	    case CRSTATE:
		break;
#endif
	    default:
		if (j < ROOTINO)
		    break;
		errexit("BAD STATE %d FOR INODE I=%d", statemap[j], j);
	    }
	}
	if (c == 0)
	    for (i = 0; i < ROOTINO; i++) {
#ifdef AFS_NEWCG_ENV
		setbit(cg_inosused(newcg), i);
#else
		setbit(newcg->cg_iused, i);
#endif /* AFS_NEWCG_ENV */
		newcg->cg_cs.cs_nifree--;
	    }
	for (i = 0, d = dbase; d < dmax; d += fs->fs_frag, i += fs->fs_frag) {
	    frags = 0;
	    for (j = 0; j < fs->fs_frag; j++) {
		if (testbmap(d + j))
		    continue;
#ifdef AFS_NEWCG_ENV
		setbit(cg_blksfree(newcg), i + j);
#else /* AFS_NEWCG_ENV */
		setbit(newcg->cg_free, i + j);
#endif /* AFS_NEWCG_ENV */
		frags++;
	    }
	    if (frags == fs->fs_frag) {
		newcg->cg_cs.cs_nbfree++;
		j = cbtocylno(fs, i);
#ifdef AFS_NEWCG_ENV
		cg_blktot(newcg)[j]++;
		cg_blks(fs, newcg, j)[cbtorpos(fs, i)]++;
#else /* AFS_NEWCG_ENV */
		newcg->cg_btot[j]++;
		newcg->cg_b[j][cbtorpos(fs, i)]++;
#endif /* AFS_NEWCG_ENV */
	    } else if (frags > 0) {
		newcg->cg_cs.cs_nffree += frags;
#ifdef AFS_NEWCG_ENV
		blk = blkmap(fs, cg_blksfree(newcg), i);
#else /* AFS_NEWCG_ENV */
		blk = blkmap(fs, newcg->cg_free, i);
#endif /* AFS_NEWCG_ENV */
		fragacct(fs, blk, newcg->cg_frsum, 1);
	    }
	}
	cstotal.cs_nffree += newcg->cg_cs.cs_nffree;
	cstotal.cs_nbfree += newcg->cg_cs.cs_nbfree;
	cstotal.cs_nifree += newcg->cg_cs.cs_nifree;
	cstotal.cs_ndir += newcg->cg_cs.cs_ndir;
	cs = &fs->fs_cs(fs, c);
	if (memcmp((char *)&newcg->cg_cs, (char *)cs, sizeof *cs) != 0
	    && dofix(&idesc[0],
		     "FREE BLK COUNT(S) WRONG IN CYL GROUP (SUPERBLK)")) {
	    memcpy((char *)cs, (char *)&newcg->cg_cs, sizeof *cs);
	    sbdirty();
	}
#ifdef AFS_NEWCG_ENV
	if (cvtflag) {
	    memcpy((char *)cg, (char *)newcg, (int)fs->fs_cgsize);
	    cgdirty();
	    continue;
	}
#endif /* AFS_NEWCG_ENV */
#ifdef AFS_NEWCG_ENV
	if (memcmp(cg_inosused(newcg), cg_inosused(cg), mapsize) != 0
	    && dofix(&idesc[1], "BLK(S) MISSING IN BIT MAPS")) {
	    memcpy(cg_inosused(cg), cg_inosused(newcg), mapsize);
	    cgdirty();
	}
#else /* AFS_NEWCG_ENV */
	if (memcmp(newcg->cg_iused, cg->cg_iused, mapsize) != 0
	    && dofix(&idesc[1], "BLK(S) MISSING IN BIT MAPS")) {
	    memcpy(cg->cg_iused, newcg->cg_iused, mapsize);
	    cgdirty();
	}
#endif /* AFS_NEWCG_ENV */
	if ((memcmp((char *)newcg, (char *)cg, basesize) != 0 ||
#ifdef AFS_NEWCG_ENV
	     memcmp((char *)&cg_blktot(newcg)[0], (char *)&cg_blktot(cg)[0],
		    sumsize) != 0) &&
#else /* AFS_NEWCG_ENV */
	     memcmp((char *)newcg->cg_btot, (char *)cg->cg_btot,
		    sumsize) != 0) &&
#endif /* AFS_NEWCG_ENV */
	    dofix(&idesc[2], "SUMMARY INFORMATION BAD")) {
#ifdef AFS_NEWCG_ENV
	    memcpy((char *)cg, (char *)newcg, basesize);
	    memcpy((char *)&cg_blktot(cg)[0], (char *)&cg_blktot(newcg)[0],
		   sumsize);
#else /* AFS_NEWCG_ENV */
	    memcpy((char *)cg, (char *)newcg, basesize);
	    memcpy((char *)cg->cg_btot, (char *)newcg->cg_btot, sumsize);
#endif /* AFS_NEWCG_ENV */
	    cgdirty();
	}
    }
#ifdef AFS_NEWCG_ENV
    if (fs->fs_postblformat == FS_42POSTBLFMT)
	fs->fs_nrpos = savednrpos;
#endif /* AFS_NEWCG_ENV */
    if (memcmp((char *)&cstotal, (char *)&fs->fs_cstotal, sizeof *cs) != 0
	&& dofix(&idesc[0], "FREE BLK COUNT(S) WRONG IN SUPERBLK")) {
	memcpy((char *)&fs->fs_cstotal, (char *)&cstotal, sizeof *cs);
	fs->fs_ronly = 0;
	sbfine(fs);
	sbdirty();
    }
}

/* returns true if sbdirty should be called */
sbfine(fs)
     struct fs *fs;
{
    int rcode;
    rcode = 0;
    if (fs->fs_fmod != 0) {
	fs->fs_fmod = 0;
	rcode = 1;
    }
    return rcode;
}
