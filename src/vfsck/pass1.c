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
extern int ge_danger;
#define	DUX
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

static daddr_t badblk;
static daddr_t dupblk;
int pass1check();
static int oldreported;

pass1()
{
    int c, i, j;
    struct dinode *dp;
    struct zlncnt *zlnp;
    int ndb, cgd;
    struct inodesc idesc;
    ino_t inumber;

    /*
     * Set file system reserved blocks in used block map.
     */
    for (c = 0; c < sblock.fs_ncg; c++) {
	cgd = cgdmin(&sblock, c);
	if (c == 0) {
	    i = cgbase(&sblock, c);
	    cgd += howmany(sblock.fs_cssize, sblock.fs_fsize);
	} else
	    i = cgsblock(&sblock, c);
	for (; i < cgd; i++)
	    setbmap(i);
    }
    /*
     * Find all allocated blocks.
     */
    memset(&idesc, 0, sizeof(struct inodesc));
    idesc.id_type = ADDR;
    idesc.id_func = pass1check;
    inumber = 0;
    n_files = n_blks = 0;
#ifdef VICE
    nViceFiles = 0;
#endif /* VICE */
    for (c = 0; c < sblock.fs_ncg; c++) {
	for (i = 0; i < sblock.fs_ipg; i++, inumber++) {
	    if (inumber < ROOTINO)
		continue;
	    dp = ginode(inumber);
	    if ((dp->di_mode & IFMT) == 0) {
		if (memcmp
		    ((char *)dp->di_db, (char *)zino.di_db,
		     NDADDR * sizeof(daddr_t))
		    || memcmp((char *)dp->di_ib, (char *)zino.di_ib,
			      NIADDR * sizeof(daddr_t)) ||
#if defined(ACLS) && defined(AFS_HPUX_ENV)
		    dp->di_mode || dp->di_size || dp->di_contin) {
		    if (dp->di_contin != 0)
			pwarn("UNALLOCATED INODE HAS BAD ic_contin VALUE %d",
			      dp->di_contin);
		    else
#else
		    dp->di_mode || dp->di_size) {
#endif

		    pfatal("PARTIALLY ALLOCATED INODE I=%u", inumber);
		    if (reply("CLEAR") == 1) {
#ifdef VICE
			zapino(dp);
#else /* VICE */
			clearinode(dp);
#endif /* VICE */
			inodirty();
		    }
		}
		statemap[inumber] = USTATE;
		continue;
	    }
	    lastino = inumber;
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	    /*
	     * Don't check blocks and sizes of
	     * continuation inodes
	     */
	    if (CONT) {
		statemap[inumber] = CSTATE;
		lncntp[inumber] = dp->di_nlink;
		n_cont++;
		continue;
	    }
#endif /* ACLS */
#if	defined(AFS_SUN56_ENV)
	    if (dp->di_size < 0 || dp->di_size > (UOFF_T) UFS_MAXOFFSET_T) {
		if (debug)
		    printf("bad size %llu:", dp->di_size);
		goto unknown;
	    }
#else
	    if (dp->di_size < 0 || dp->di_size + sblock.fs_bsize - 1 < 0) {
		if (debug)
		    printf("bad size %d:", dp->di_size);
		goto unknown;
	    }
#endif
#if defined(AFS_HPUX_ENV)
	    /* initialize all R/W activities of FIFO file */
	    /* make sure FIFO is empty (everything is 0) */
	    if ((dp->di_mode & IFMT) == IFIFO
		&& (dp->di_frcnt != 0 || dp->di_fwcnt != 0)) {
		if (!qflag)
		    pwarn("NON-ZERO READER/WRITER COUNT(S) ON PIPE I=%u",
			  inumber);
		if (preen && !qflag)
		    printf(" (CORRECTED)\n");
		else if (!qflag) {
		    if (reply("CORRECT") == 0)
			goto no_reset;
		}
		dp->di_size = 0;
		dp->di_frptr = 0;
		dp->di_fwptr = 0;
		dp->di_frcnt = 0;
		dp->di_fwcnt = 0;
		dp->di_fflag = 0;
		dp->di_fifosize = 0;
		inodirty();
		ndb = 0;
		for (j = ndb; j < NDADDR; j++)
		    dp->di_db[j] = 0;
	    }
#ifdef IC_FASTLINK
	    else if (FASTLNK) {
		/*
		 * Fast symlink -- verify that the size is valid and that the length
		 * of the path is correct.
		 */

		if (dp->di_size >= MAX_FASTLINK_SIZE) {
		    if (debug)
			printf("bad fastlink size %d:", dp->di_size);
		    goto unknown;
		}
		dp->di_symlink[MAX_FASTLINK_SIZE - 1] = '\0';
		if (strlen(dp->di_symlink) != dp->di_size) {
		    int len = strlen(dp->di_symlink);
		    pwarn("BAD SYMLINK SIZE, SHOULD BE %d: size = %d", len,
			  dp->di_size);
		    if (preen)
			printf(" (CORRECTED)\n");
		    else {
			printf("\n");
			pinode(inumber);
			if (reply("CORRECT") == 0)
			    continue;
		    }
		    dp->di_size = len;
		    inodirty();
		}
		goto ignore_direct_block_check;
	    }
#endif /* IC_FASTLINK */
#endif
	  no_reset:
	    if (!preen && (dp->di_mode & IFMT) == IFMT
		&& reply("HOLD BAD BLOCK") == 1) {
		dp->di_size = sblock.fs_fsize;
		dp->di_mode = IFREG | 0600;
		inodirty();
	    }
	    ndb = howmany(dp->di_size, (UOFF_T) sblock.fs_bsize);
#ifdef	AFS_SUN5_ENV
	    if (dp->di_oeftflag == oEFT_MAGIC) {
		dp->di_oeftflag = 0;	/* XXX migration aid */
		inodirty();
	    }
#endif

	    if (ndb < 0) {
		if (debug)
#if	defined(AFS_SUN56_ENV)
		    printf("bad size %" AFS_INT64_FMT " ndb %d:",
#else
		    printf("bad size %d ndb %d:",
#endif
			   dp->di_size, ndb);
		goto unknown;
	    }
	    if ((dp->di_mode & IFMT) == IFBLK
		|| (dp->di_mode & IFMT) == IFCHR)
		ndb++;
#ifdef	AFS_OSF_ENV
	    if ((dp->di_flags & IC_FASTLINK) == 0) {
#endif /* AFS_OSF_ENV */
		for (j = ndb; j < NDADDR; j++) {
#if defined(AFS_HPUX_ENV) && (defined(DUX) || defined(CNODE_DEV))
		    /*
		     * DUX uses db[2] on cnode-specific
		     * device files, so skip 'em
		     */
		    if (j == 2 && SPECIAL)
			continue;
#endif
		    if (dp->di_db[j] != 0) {
			if (debug)
			    printf("bad direct addr: %d\n", dp->di_db[j]);
			goto unknown;
		    }
		}
		for (j = 0, ndb -= NDADDR; ndb > 0; j++)
		    ndb /= NINDIR(&sblock);
		for (; j < NIADDR; j++)
		    if (dp->di_ib[j] != 0) {
#ifdef	AFS_HPUX_ENV
			if ((dp->di_mode & IFMT) != IFIFO) {
#endif
			    if (debug)
				printf("bad indirect addr: %d\n",
				       dp->di_ib[j]);
			    goto unknown;
#ifdef	AFS_HPUX_ENV
			}
#endif

		    }
#if	defined(AFS_HPUX_ENV)
	      ignore_direct_block_check:
#endif
#ifdef	AFS_OSF_ENV
	    }
#endif /* AFS_OSF_ENV */
	    if (ftypeok(dp) == 0)
		goto unknown;
	    n_files++;
	    lncntp[inumber] = dp->di_nlink;
	    if (dp->di_nlink <= 0) {
		zlnp = (struct zlncnt *)malloc(sizeof *zlnp);
		if (zlnp == NULL) {
		    pfatal("LINK COUNT TABLE OVERFLOW");
		    if (reply("CONTINUE") == 0)
			errexit("");
		} else {
		    zlnp->zlncnt = inumber;
		    zlnp->next = zlnhead;
		    zlnhead = zlnp;
		}
	    }
#if	defined(AFS_SUN56_ENV)
	    if (OLDVICEINODE) {
		if (yflag) {
		    if (!oldreported) {
			printf
			    ("This vicep partition seems to contain pre Sol2.6 AFS inodes\n");
			printf
			    ("You should run the AFS file conversion utility before installing Sol 2.6\n");
			printf("Continuing anyway.\n");
			oldreported++;
		    }
		} else {
		    /* This looks like a sol 2.5 AFS inode */
		    printf
			("This vicep partition seems to contain pre Sol2.6 AFS inodes\n");
		    printf
			("You should run the AFS file conversion utility before installing Sol 2.6\n");
		    exit(100);	/* unique return code? */
		}
	    }
#endif
	    statemap[inumber] =
#ifdef VICE
		(dp->di_mode & IFMT) ==
		IFDIR ? DSTATE : (VICEINODE ? VSTATE : FSTATE);
#else /* VICE */
		(dp->di_mode & IFMT) == IFDIR ? DSTATE : FSTATE;
#endif /* VICE */
#if defined(ACLS) && defined(AFS_HPUX_ENV)
	    /*
	     * keep track of associated contin inodes
	     */
	    if (dp->di_contin != 0)
		statemap[inumber] |= HASCINODE;
#endif /* ACLS */
	    badblk = dupblk = 0;
	    idesc.id_number = inumber;
	    idesc.id_entryno = 0;
#ifdef	AFS_SUN5_ENV
	    idesc.id_fix = DONTKNOW;
#endif
	    (void)ckinode(dp, &idesc);

	    idesc.id_entryno *= btodb(sblock.fs_fsize);

	    if (dp->di_blocks != idesc.id_entryno) {
		pwarn("INCORRECT BLOCK COUNT I=%u (%ld should be %ld)",
		      inumber, dp->di_blocks, idesc.id_entryno);
		if (preen)
		    printf(" (CORRECTED)\n");
		else if (reply("CORRECT") == 0)
		    continue;
#ifdef	AFS_SUN5_ENV
		dp = ginode(inumber);
#endif
		dp->di_blocks = idesc.id_entryno;
		inodirty();
	    }
#ifdef	AFS_SUN5_ENV
	    if ((dp->di_mode & IFMT) == IFDIR)
		if (dp->di_blocks == 0)
		    statemap[inumber] = DCLEAR;
#endif
	    continue;
	  unknown:
	    pfatal("UNKNOWN FILE TYPE I=%u", inumber);
#ifdef	AFS_SUN5_ENV
	    if ((dp->di_mode & IFMT) == IFDIR) {
		statemap[inumber] = DCLEAR;
#ifdef	notdef
		cacheino(dp, inumber);
#endif
	    } else
#endif
		statemap[inumber] = FCLEAR;
	    if (reply("CLEAR") == 1) {
		statemap[inumber] = USTATE;
#ifdef VICE
		zapino(dp);
#else /* VICE */
		clearinode(dp);
#endif /* VICE */
		inodirty();
	    }
	}
    }
}

pass1check(idesc)
     struct inodesc *idesc;
{
    int res = KEEPON;
    int anyout, nfrags;
    daddr_t blkno = idesc->id_blkno;
    struct dups *dlp;
    struct dups *new;

    if ((anyout = chkrange(blkno, idesc->id_numfrags)) != 0) {
	blkerror(idesc->id_number, "BAD", blkno);
	if (++badblk >= MAXBAD) {
	    pwarn("EXCESSIVE BAD BLKS I=%u", idesc->id_number);
	    if (preen)
		printf(" (SKIPPING)\n");
	    else if (reply("CONTINUE") == 0)
		errexit("");
#ifdef	AFS_HPUX_ENV
	    ge_danger = 1;
#endif
	    return (STOP);
	}
    }
    for (nfrags = idesc->id_numfrags; nfrags > 0; blkno++, nfrags--) {
	if (anyout && chkrange(blkno, 1)) {
	    res = SKIP;
	} else if (!testbmap(blkno)) {
	    n_blks++;
	    setbmap(blkno);
	} else {
	    blkerror(idesc->id_number, "DUP", blkno);
	    if (++dupblk >= MAXDUP) {
		pwarn("EXCESSIVE DUP BLKS I=%u", idesc->id_number);
		if (preen)
		    printf(" (SKIPPING)\n");
		else if (reply("CONTINUE") == 0)
		    errexit("");
#ifdef	AFS_HPUX_ENV
		ge_danger = 1;
#endif
		return (STOP);
	    }
	    new = (struct dups *)malloc(sizeof(struct dups));
	    if (new == NULL) {
		pfatal("DUP TABLE OVERFLOW.");
		if (reply("CONTINUE") == 0)
		    errexit("");
		return (STOP);
	    }
	    new->dup = blkno;
	    if (muldup == 0) {
		duplist = muldup = new;
		new->next = 0;
	    } else {
		new->next = muldup->next;
		muldup->next = new;
	    }
	    for (dlp = duplist; dlp != muldup; dlp = dlp->next)
		if (dlp->dup == blkno)
		    break;
	    if (dlp == muldup && dlp->dup != blkno)
		muldup = new;
	}
	/*
	 * count the number of blocks found in id_entryno
	 */
	idesc->id_entryno++;
    }
    return (res);
}
