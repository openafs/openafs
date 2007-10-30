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

#include <stdio.h>
#include <string.h>
#define VICE

#if	defined(AFS_SUN_ENV) || defined(AFS_OSF_ENV)
#define AFS_NEWCG_ENV
#else
#undef AFS_NEWCG_ENV
#endif

#ifdef VICE
#define msgprintf vfscklogprintf
extern vfscklogprintf();
#endif

#define DKTYPENAMES
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

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <ctype.h>
#ifdef	AFS_SUN5_ENV
#include <sys/mntent.h>
#include <sys/vfstab.h>
#endif

#include <afs/osi_inode.h>
#include "fsck.h"

struct bufarea asblk;
struct bufarea *pbp;

#define altsblock (*asblk.b_un.b_fs)
#define POWEROF2(num)	(((num) & ((num) - 1)) == 0)

/*
 * The size of a cylinder group is calculated by CGSIZE. The maximum size
 * is limited by the fact that cylinder groups are at most one block.
 * Its size is derived from the size of the maps maintained in the 
 * cylinder group and the (struct cg) size.
 */
#define CGSIZE(fs) \
    /* base cg */	(sizeof(struct cg) + \
    /* blktot size */	(fs)->fs_cpg * sizeof(afs_int32) + \
    /* blks size */	(fs)->fs_cpg * (fs)->fs_nrpos * sizeof(short) + \
    /* inode map */	howmany((fs)->fs_ipg, NBBY) + \
    /* block map */	howmany((fs)->fs_cpg * (fs)->fs_spc / NSPF(fs), NBBY))

char *malloc(), *calloc();
struct disklabel *getdisklabel();

setup(dev)
     char *dev;
{
    dev_t rootdev;
    long cg, size, asked, i, j;
    long bmapsize;
    struct disklabel *lp;
    struct stat statb;
    struct fs proto;
#if	defined(AFS_SUN5_ENV)
    static char sname[MAXPATHLEN];
#endif

    /* reset static variables that may have been damaged by parent fork */
    mlk_pbp = 0;
    pbp = 0;
    mlk_startinum = 0;
#if defined(ACLS) && defined(AFS_HPUX_ENV)
    n_cont = 0;
#endif
    havesb = 0;
    if (stat("/", &statb) < 0)
	errexit("Can't stat root\n");
    rootdev = statb.st_dev;
#if	defined(AFS_SUN5_ENV)
    strncpy(sname, dev, sizeof(sname));
  restat:
    if (stat(sname, &statb) < 0) {
	perror(sname);
	msgprintf("Can't stat %s\n", sname);
#else
    if (stat(dev, &statb) < 0) {
	perror(dev);
	printf("Can't stat %s\n", dev);
#endif
	return (0);
    }
#ifdef	AFS_SUN5_ENV
    if ((statb.st_mode & S_IFMT) == S_IFDIR) {
	FILE *fp;
	struct vfstab vtab;

	if ((fp = fopen(VFSTAB, "r")) == NULL) {
	    pfatal("Can't open %s file\n", VFSTAB);
	    exit(1);
	}
	while (!getvfsent(fp, &vtab)) {
	    if (vtab.vfs_mountp && !strcmp(vtab.vfs_mountp, sname)) {
		strcpy(sname, vtab.vfs_special);
		if (rflag) {
		    char *s = vtab.vfs_fsckdev;
		    strcpy(sname, s);
		}
		goto restat;
	    }
	}
	fclose(fp);
    }
#endif
    if ((statb.st_mode & S_IFMT) != S_IFBLK
	&& (statb.st_mode & S_IFMT) != S_IFCHR) {
	pfatal("device is not a block or character device");
	if (reply("file is not a block or character device; OK") == 0)
	    return (0);
    }
#ifdef	AFS_SUN5_ENV
    if (mounted(sname))
	if (rflag)
	    mountedfs++;
	else {
	    printf("%s is mounted, fsck on BLOCK device ignored\n", sname);
	    exit(33);
	}
    if (rflag) {
	char bname[MAXPATHLEN], *tname;

	strcpy(bname, sname);
	tname = unrawname(bname);
	if (stat(tname, &statb) < 0) {
	    pfatal("Can't stat %s\n", tname);
	    return (0);
	}
	dev = sname;
    }
#endif
    hotroot = is_hotroot(dev);

    /*
     * The following code is added to improve usability of fsck.
     * we need to give user a warning if the device being checked is
     * a hotroot, a mounted file system, or a swap device.
     * The rules are:
     *      1) if nflag is set, it's pretty safe to fsck the target dev
     *      2) if the target device is a swap, exit
     *      3) if hotroot is set, and "-F" is not specified prompt the 
     *              user and wait for reply
     *      4) if the target is a mounted file system, and "-F" is not
     *              specified, prompt the user and wait for reply
     *      5) if the "-m" is specifed, do no prompting since only a sanity
     *              check is being done. important during booting
     *
     * Caveat: There is no way to tell the current run level, so we cannot
     * tell whether or not we are in single user mode.
     */
    if (!nflag) {
	int mounted, swap;
	struct stat st_mounted;

	mounted = swap = 0;
	if (!hotroot) {
	    mounted = is_mounted(dev, &st_mounted);
#ifdef	AFS_HPUX_ENV
	    swap = is_swap(st_mounted.st_rdev);
#endif
	}
	if (!fflag
#ifdef  AFS_HPUX_ENV
	    && !mflag
#endif
	    ) {
	    if (hotroot) {
		msgprintf("fsck: %s: root file system", dev);
		if (preen)
		    msgprintf(" (CONTINUING)\n");
		else {
		    if (!freply("continue (y/n)"))
			return (0);
		}
	    } else if (mounted) {
		msgprintf("fsck: %s: mounted file system", dev);
		if (preen)
		    msgprintf(" (CONTINUING)\n");
		else {
		    if (!freply("continue (y/n)"))
			return (0);
		}
	    } else if (swap) {
		msgprintf("fsck: %s: swap device\n", dev);
		if (preen)
		    msgprintf(" (CONTINUING)\n");
		else {
		    if (!freply("continue (y/n)"))
			return (0);
		}
	    }
	}
    }
    if (rootdev == statb.st_rdev)
	hotroot++;
    if ((fsreadfd = open(dev, O_RDONLY)) < 0) {
	perror(dev);
	msgprintf("Can't open %s\n", dev);
	return (0);
    }
    if (preen == 0)
	msgprintf("** %s", dev);
    if (nflag || (fswritefd = open(dev, O_WRONLY)) < 0) {
	fswritefd = -1;
	msgprintf(" (NO WRITE)");
#ifdef	notdef
	if (preen) {
	    pfatal("NO WRITE ACCESS");
	}
#endif
    }
    if (preen == 0)
	msgprintf("\n");
#if	defined(AFS_HPUX101_ENV)
    setup_all_block_seek();
#endif
#ifdef	AFS_SUN5_ENV
    if (!preen && debug)
	printf(" pid %d\n", getpid());
    if (debug && (hotroot || mountedfs)) {
	printf("** %s", sname);
	if (hotroot) {
	    printf(" is root fs");
	    if (mountedfs)
		printf(" and");
	}
	if (mountedfs)
	    printf(" is mounted");
	printf(".\n");
    }
#endif
    fsmodified = 0;
    lfdir = 0;
    initbarea(&sblk);
    initbarea(&asblk);
#ifdef __alpha
    sblk.b_un.b_buf = malloc(SBSIZE + ALPHA_EXT);
    asblk.b_un.b_buf = malloc(SBSIZE + ALPHA_EXT);
#else /* __alpha */
    sblk.b_un.b_buf = malloc(SBSIZE);
    asblk.b_un.b_buf = malloc(SBSIZE);
#endif
    if (sblk.b_un.b_buf == NULL || asblk.b_un.b_buf == NULL)
	errexit("cannot allocate space for superblock\n");
    dev_bsize = secsize = DEV_BSIZE;

    /*
     * Read in the superblock, looking for alternates if necessary
     */
    if (readsb(1) == 0) {
	if (bflag || preen || calcsb(dev, fsreadfd, &proto) == 0)
	    return (0);
	if (reply("LOOK FOR ALTERNATE SUPERBLOCKS") == 0)
	    return (0);
	for (cg = 0; cg < proto.fs_ncg; cg++) {
	    bflag = fsbtodb(&proto, cgsblock(&proto, cg));
	    if (readsb(0) != 0)
		break;
	}
	if (cg >= proto.fs_ncg) {
	    msgprintf("%s %s\n%s %s\n%s %s\n",
		      "SEARCH FOR ALTERNATE SUPER-BLOCK",
		      "FAILED. YOU MUST USE THE",
		      "-b OPTION TO FSCK TO SPECIFY THE",
		      "LOCATION OF AN ALTERNATE",
		      "SUPER-BLOCK TO SUPPLY NEEDED",
		      "INFORMATION; SEE fsck(8).");
	    return (0);
	}
	pwarn("USING ALTERNATE SUPERBLOCK AT %d\n", bflag);
    }
    maxfsblock = sblock.fs_size;
    maxino = sblock.fs_ncg * sblock.fs_ipg;
    /*
     * Check and potentially fix certain fields in the super block.
     */
    if (sblock.fs_optim != FS_OPTTIME && sblock.fs_optim != FS_OPTSPACE) {
	pfatal("UNDEFINED OPTIMIZATION IN SUPERBLOCK");
	if (reply("SET TO DEFAULT") == 1) {
	    sblock.fs_optim = FS_OPTTIME;
	    sbdirty();
	}
    }
    if ((sblock.fs_minfree < 0 || sblock.fs_minfree > 99)) {
	pfatal("IMPOSSIBLE MINFREE=%d IN SUPERBLOCK", sblock.fs_minfree);
	if (reply("SET TO DEFAULT") == 1) {
	    sblock.fs_minfree = 10;
	    sbdirty();
	}
    }
#ifdef	AFS_HPUX_ENV
    /*
     * Do we need to continue ?
     */
    if (pclean && sblock.fs_clean == FS_CLEAN)
	return (-1);
    if (pclean && hotroot && sblock.fs_clean == FS_OK)
	return (-1);
#endif
#ifdef AFS_NEWCG_ENV
# ifndef AFS_SUN59_ENV
    if (sblock.fs_interleave < 1) {
	pwarn("IMPOSSIBLE INTERLEAVE=%d IN SUPERBLOCK", sblock.fs_interleave);
	sblock.fs_interleave = 1;
	if (preen)
	    msgprintf(" (FIXED)\n");
	if (preen || reply("SET TO DEFAULT") == 1) {
	    sbdirty();
	    dirty(&asblk);
	}
    }
# endif /* AFS_SUN59_ENV */
#endif /* AFS_NEWCG_ENV */
#ifdef AFS_NEWCG_ENV
    if (sblock.fs_npsect < sblock.fs_nsect) {
	pwarn("IMPOSSIBLE NPSECT=%d IN SUPERBLOCK", sblock.fs_npsect);
	sblock.fs_npsect = sblock.fs_nsect;
	if (preen)
	    msgprintf(" (FIXED)\n");
	if (preen || reply("SET TO DEFAULT") == 1) {
	    sbdirty();
	    dirty(&asblk);
	}
    }
#endif /* AFS_NEWCG_ENV */
#ifdef AFS_NEWCG_ENV
    if (cvtflag) {
	if (sblock.fs_postblformat == FS_42POSTBLFMT) {
	    /*
	     * Requested to convert from old format to new format
	     */
	    if (preen)
		pwarn("CONVERTING TO NEW FILE SYSTEM FORMAT\n");
	    else if (!reply("CONVERT TO NEW FILE SYSTEM FORMAT"))
		return (0);
	    isconvert = 1;
	    sblock.fs_postblformat = FS_DYNAMICPOSTBLFMT;
	    sblock.fs_nrpos = 8;
	    sblock.fs_postbloff =
		(char *)(&sblock.fs_opostbl[0][0]) -
		(char *)(&sblock.fs_link);
	    sblock.fs_rotbloff =
		&sblock.fs_space[0] - (u_char *) (&sblock.fs_link);
	    sblock.fs_cgsize = fragroundup(&sblock, CGSIZE(&sblock));
	    /*
	     * Planning now for future expansion.
	     */
#			if (BYTE_ORDER == BIG_ENDIAN)
	    sblock.fs_qbmask.val[0] = 0;
	    sblock.fs_qbmask.val[1] = ~sblock.fs_bmask;
	    sblock.fs_qfmask.val[0] = 0;
	    sblock.fs_qfmask.val[1] = ~sblock.fs_fmask;
#			endif /* BIG_ENDIAN */
#			if (BYTE_ORDER == LITTLE_ENDIAN)
	    sblock.fs_qbmask.val[0] = ~sblock.fs_bmask;
	    sblock.fs_qbmask.val[1] = 0;
	    sblock.fs_qfmask.val[0] = ~sblock.fs_fmask;
	    sblock.fs_qfmask.val[1] = 0;
#			endif /* LITTLE_ENDIAN */
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN3_ENV)
#ifdef	AFS_SUN5_ENV
	    sblock.fs_state = FSOKAY - sblock.fs_time;	/* make mountable */
#else
	    fs_set_state(&sblock, FSOKAY - sblock.fs_time);
#endif
	    sblock.fs_clean = FSCLEAN;
#endif
	    sbdirty();
	    dirty(&asblk);
	} else if (sblock.fs_postblformat == FS_DYNAMICPOSTBLFMT) {
	    /*
	     * Requested to convert from new format to old format
	     */
	    if (sblock.fs_nrpos != 8 || sblock.fs_ipg > 2048
		|| sblock.fs_cpg > 32 || sblock.fs_cpc > 16) {
		msgprintf("PARAMETERS OF CURRENT FILE SYSTEM DO NOT\n\t");
		msgprintf("ALLOW CONVERSION TO OLD FILE SYSTEM FORMAT\n");
		errexit("");

	    }
	    if (preen)
		pwarn("CONVERTING TO OLD FILE SYSTEM FORMAT\n");
	    else if (!reply("CONVERT TO OLD FILE SYSTEM FORMAT"))
		return (0);
	    isconvert = 1;
	    sblock.fs_postblformat = FS_42POSTBLFMT;
	    sblock.fs_cgsize =
		fragroundup(&sblock,
			    sizeof(struct ocg) + howmany(sblock.fs_fpg,
							 NBBY));
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN3_ENV)
#ifdef	AFS_SUN5_ENV
	    sblock.fs_npsect = 0;
# ifndef AFS_SUN59_ENV
	    sblock.fs_interleave = 0;
# endif
	    sblock.fs_state = FSOKAY - sblock.fs_time;	/* make mountable */
#else
	    fs_set_state(&sblock, FSOKAY - sblock.fs_time);
#endif
	    sblock.fs_clean = FSCLEAN;
#endif
	    sbdirty();
	    dirty(&asblk);
	} else {
	    errexit("UNKNOWN FILE SYSTEM FORMAT\n");
	}
    }
#endif /* AFS_NEWCG_ENV */
    if (asblk.b_dirty) {
	memcpy((char *)&altsblock, (char *)&sblock, (int)sblock.fs_sbsize);
	flush(fswritefd, &asblk);
    }
    /*
     * read in the summary info.
     */
    asked = 0;
#if	defined(AFS_SUN56_ENV)
    {
	caddr_t sip;
	sip = calloc(1, sblock.fs_cssize);
	sblock.fs_u.fs_csp = (struct csum *)sip;
	for (i = 0, j = 0; i < sblock.fs_cssize; i += sblock.fs_bsize, j++) {
	    size =
		sblock.fs_cssize - i <
		sblock.fs_bsize ? sblock.fs_cssize - i : sblock.fs_bsize;
	    if (bread
		(fsreadfd, sip,
		 fsbtodb(&sblock, sblock.fs_csaddr + j * sblock.fs_frag),
		 size) != 0 && !asked) {
		pfatal("BAD SUMMARY INFORMATION");
		if (reply("CONTINUE") == 0)
		    errexit("");
		return (0);
	    }
	    sip += size;
	}
    }
#else /* AFS_SUN56_ENV */
#if     defined(AFS_HPUX110_ENV)
    size = fragroundup(&sblock, sblock.fs_cssize);
    sblock.fs_csp = (struct csum *)calloc(1, (unsigned)size);
    if ((bread
	 (fsreadfd, (char *)sblock.fs_csp, fsbtodb(&sblock, sblock.fs_csaddr),
	  size) != 0) && !asked) {
	pfatal("BAD SUMMARY INFORMATION");
	if (reply("CONTINUE") == 0)
	    errexit("");
	asked++;
    }
#else /* AFS_HPUX110_ENV */
#if	defined(AFS_HPUX101_ENV)
    {
	j = 0;
	size = fragroundup(&sblock, sblock.fs_cssize);
#else
    for (i = 0, j = 0; i < sblock.fs_cssize; i += sblock.fs_bsize, j++) {
	size =
	    sblock.fs_cssize - i <
	    sblock.fs_bsize ? sblock.fs_cssize - i : sblock.fs_bsize;
#endif /* AFS_HPUX101_ENV */
	sblock.fs_csp[j] = (struct csum *)calloc(1, (unsigned)size);
	if (bread
	    (fsreadfd, (char *)sblock.fs_csp[j],
	     fsbtodb(&sblock, sblock.fs_csaddr + j * sblock.fs_frag),
	     size) != 0 && !asked) {
	    pfatal("BAD SUMMARY INFORMATION");
	    if (reply("CONTINUE") == 0)
		errexit("");
#ifdef	AFS_SUN_ENV
	    return (0);
#else
	    asked++;
#endif
	}
    }
#endif /* else AFS_HPUX110_ENV */
#endif /* else AFS_SUN56_ENV */

#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN3_ENV)
    /*
     * if not forced, preening, not converting, and is clean; stop checking
     */
#ifdef	AFS_SUN5_ENV
    if ((fflag == 0) && preen && (isconvert == 0)
	&& (FSOKAY == (sblock.fs_state + sblock.fs_time)) &&
#else
    if (preen && (isconvert == 0)
	&& (FSOKAY == (fs_get_state(&sblock) + sblock.fs_time)) &&
#endif
	((sblock.fs_clean == FSCLEAN) || (sblock.fs_clean == FSSTABLE))) {
	iscorrupt = 0;
	printclean();
	return (0);
    }
#endif
#ifdef	AFS_OSF_ENV
    if (!fflag && !bflag && !nflag && !hotroot && sblock.fs_clean == FS_CLEAN
	&& !sblk.b_dirty) {
	pwarn("Clean file system - skipping fsck\n");
	return (FS_CLEAN);
    }
#endif /* AFS_OSF_ENV */

    /*
     * allocate and initialize the necessary maps
     */
    bmapsize = roundup(howmany(maxfsblock, NBBY), sizeof(short));
    blockmap = calloc((unsigned)bmapsize, sizeof(char));
    if (blockmap == NULL) {
	msgprintf("cannot alloc %d bytes for blockmap\n", bmapsize);
	goto badsb;
    }
    statemap = calloc((unsigned)(maxino + 1), sizeof(char));
    if (statemap == NULL) {
	msgprintf("cannot alloc %d bytes for statemap\n", maxino + 1);
	goto badsb;
    }
    lncntp = (short *)calloc((unsigned)(maxino + 1), sizeof(short));
    if (lncntp == NULL) {
	msgprintf("cannot alloc %d bytes for lncntp\n",
		  (maxino + 1) * sizeof(short));
	goto badsb;
    }

    bufinit();
    return (1);

  badsb:
    ckfini();
    return (0);
}

/*
 * Read in the super block and its summary info.
 */
readsb(listerr)
     int listerr;
{
#ifdef AFS_NEWCG_ENV
    daddr_t super = bflag ? bflag : SBOFF / dev_bsize;
#else /* AFS_NEWCG_ENV */
    daddr_t super = bflag ? bflag : SBLOCK;

#endif /* AFS_NEWCG_ENV */
    if (bread(fsreadfd, (char *)&sblock, super, (long)SBSIZE) != 0)
	return (0);
    sblk.b_bno = super;
    sblk.b_size = SBSIZE;
    /*
     * run a few consistency checks of the super block
     */
#ifdef	AFS_HPUX_ENV
#if defined(FD_FSMAGIC)
    if ((sblock.fs_magic != FS_MAGIC) && (sblock.fs_magic != FS_MAGIC_LFN)
	&& (sblock.fs_magic != FD_FSMAGIC)
#if	defined(AFS_HPUX101_ENV)
	&& (sblock.fs_magic != FD_FSMAGIC_2)
#endif
	)
#else /* not new magic number */
    if ((sblock.fs_magic != FS_MAGIC) && (sblock.fs_magic != FS_MAGIC_LFN))
#endif /* new magic number */
#else
    if (sblock.fs_magic != FS_MAGIC)
#endif
    {
	badsb(listerr, "MAGIC NUMBER WRONG");
	return (0);
    }
    if (sblock.fs_ncg < 1) {
	badsb(listerr, "NCG OUT OF RANGE");
	return (0);
    }
    if (sblock.fs_cpg < 1) {
	printf("fs_cpg= %d\n", sblock.fs_cpg);
	badsb(listerr, "CPG OUT OF RANGE");
	return (0);
    }
    if (sblock.fs_ncg * sblock.fs_cpg < sblock.fs_ncyl
	|| (sblock.fs_ncg - 1) * sblock.fs_cpg >= sblock.fs_ncyl) {
	badsb(listerr, "NCYL LESS THAN NCG*CPG");
	return (0);
    }
    if (sblock.fs_sbsize > SBSIZE) {
	badsb(listerr, "SIZE PREPOSTEROUSLY LARGE");
	return (0);
    }
    /*
     * Compute block size that the filesystem is based on,
     * according to fsbtodb, and adjust superblock block number
     * so we can tell if this is an alternate later.
     */
    super *= dev_bsize;
    dev_bsize = sblock.fs_fsize / fsbtodb(&sblock, 1);
    sblk.b_bno = super / dev_bsize;
    /*
     * Set all possible fields that could differ, then do check
     * of whole super block against an alternate super block.
     * When an alternate super-block is specified this check is skipped.
     */
    getblk(&asblk, cgsblock(&sblock, sblock.fs_ncg - 1), sblock.fs_sbsize);
    if (asblk.b_errs)
	return (0);
    if (bflag) {
	havesb = 1;
	return (1);
    }
    altsblock.fs_link = sblock.fs_link;
#ifdef STRUCT_FS_HAS_FS_ROLLED
    altsblock.fs_rolled = sblock.fs_rolled;
#else
    altsblock.fs_rlink = sblock.fs_rlink;
#endif
    altsblock.fs_time = sblock.fs_time;
    altsblock.fs_cstotal = sblock.fs_cstotal;
    altsblock.fs_cgrotor = sblock.fs_cgrotor;
    altsblock.fs_fmod = sblock.fs_fmod;

    altsblock.fs_clean = sblock.fs_clean;
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN3_ENV)
#ifdef	AFS_SUN5_ENV
    altsblock.fs_state = sblock.fs_state;
#else
    fs_set_state(&altsblock, fs_get_state(&sblock));
#endif
#endif
    altsblock.fs_ronly = sblock.fs_ronly;
    altsblock.fs_flags = sblock.fs_flags;
    altsblock.fs_maxcontig = sblock.fs_maxcontig;
    altsblock.fs_minfree = sblock.fs_minfree;
    altsblock.fs_optim = sblock.fs_optim;
    altsblock.fs_rotdelay = sblock.fs_rotdelay;
    altsblock.fs_maxbpg = sblock.fs_maxbpg;
#if	!defined(__alpha) && !defined(AFS_SUN56_ENV)
#if !defined(AFS_HPUX110_ENV)
    /* HPUX110 will use UpdateAlternateSuper() below */
    memcpy((char *)altsblock.fs_csp, (char *)sblock.fs_csp,
	   sizeof sblock.fs_csp);
#endif /* ! AFS_HPUX110_ENV */
#endif /* ! __alpha */
#if	defined(AFS_SUN56_ENV)
    memcpy((char *)altsblock.fs_u.fs_csp_pad, (char *)sblock.fs_u.fs_csp_pad,
	   sizeof(sblock.fs_u.fs_csp_pad));
#endif
    memcpy((char *)altsblock.fs_fsmnt, (char *)sblock.fs_fsmnt,
	   sizeof sblock.fs_fsmnt);
#ifndef	AFS_HPUX_ENV
    memcpy((char *)altsblock.fs_sparecon, (char *)sblock.fs_sparecon,
	   sizeof sblock.fs_sparecon);
#endif
    /*
     * The following should not have to be copied.
     */
    altsblock.fs_fsbtodb = sblock.fs_fsbtodb;
#ifdef AFS_NEWCG_ENV
# if defined(AFS_SUN59_ENV) && defined(FS_SI_OK)
    /* fs_interleave replaced with fs_si and FS_SI_OK defined in */
    /* ufs_fs.h version 2.63 don't need to compare either */
    altsblock.fs_si = sblock.fs_si;
# else
    altsblock.fs_interleave = sblock.fs_interleave;
# endif
    altsblock.fs_npsect = sblock.fs_npsect;
    altsblock.fs_nrpos = sblock.fs_nrpos;
#endif /* AFS_NEWCG_ENV */
#if     defined(AFS_HPUX110_ENV)
    UpdateAlternateSuper(&sblock, &altsblock);
#endif /* AFS_HPUX110_ENV */
    if (memcmp((char *)&sblock, (char *)&altsblock, (int)sblock.fs_sbsize)) {
#ifdef	__alpha
	if (memcmp
	    ((char *)&sblock.fs_blank[0], (char *)&altsblock.fs_blank[0],
	     MAXCSBUFS * sizeof(int))) {
	    memset((char *)sblock.fs_blank, 0, sizeof(sblock.fs_blank));
	} else {
#endif /* __alpha */
	    badsb(listerr,
		  "VALUES IN SUPER BLOCK DISAGREE WITH THOSE IN FIRST ALTERNATE");
	    return (0);
#ifdef	__alpha
	}
#endif /* __alpha */
    }
    havesb = 1;
    return (1);
}

badsb(listerr, s)
     int listerr;
     char *s;
{

    if (!listerr)
	return;
    if (preen)
	msgprintf("%s: ", devname);
    pfatal("BAD SUPER BLOCK: %s\n", s);
#ifdef	AFS_SUN5_ENV
    pwarn("USE AN ALTERNATE SUPER-BLOCK TO SUPPLY NEEDED INFORMATION;\n");
    pwarn("eg. fsck [-F ufs] -o b=# [special ...] \n");
    pfatal("where # is the alternate super block. SEE fsck_ufs(1M). \n");
#endif
}

/* dummy function that fails if ever called */
calcsb(dev, devfd, fs)
     char *dev;
     int devfd;
     register struct fs *fs;
{
    return 0;
}

#include <sys/ustat.h>
#ifdef	AFS_HPUX_ENV
#include <sys/pstat.h>
#endif
#include <sys/errno.h>

extern int errno;

is_mounted(device, dev_st)
     char *device;
     struct stat *dev_st;
{
    char blockdev[BUFSIZ];
    struct ustat ust;
    char *dp;

    strcpy(blockdev, device);
    if (stat(blockdev, dev_st) < 0)
	return (0);

    if (is_pre_init(dev_st->st_rdev))
	return (0);

    if ((dev_st->st_mode & S_IFMT) == S_IFCHR) {
	dp = strrchr(blockdev, '/');
	if (strncmp(dp, "/r", 2) != 0)
	    while (dp >= blockdev && *--dp != '/');
	if (*(dp + 1) == 'r')
	    (void)strcpy(dp + 1, dp + 2);
	if (stat(blockdev, dev_st) < 0)
	    return (0);
    }
    if (ustat(dev_st->st_rdev, &ust) >= 0) {
	/* make sure we are not in pre_init_rc. If we are 0 should be returned in here. */
	if ((is_root(dev_st->st_rdev)) && is_roroot())
	    return (0);
	return (1);
    }
    return (0);
}

#ifdef	AFS_HPUX_ENV

#define PS_BURST 1
#ifdef AFS_HPUX102_ENV
#define PSTAT(P, N, I) pstat_getswap(P, sizeof(*P), (size_t)N, I)
#else
#define PSTAT(P, N, I) pstat(PSTAT_SWAPINFO, P, sizeof(*P), (size_t)N, I)
#endif
is_swap(devno)
     dev_t devno;
{
    struct pst_swapinfo pst[PS_BURST];
    register struct pst_swapinfo *psp = &pst[0];
    int idx = 0, count, match = 0;

    while ((count = PSTAT(psp, PS_BURST, idx) != 0)) {
	idx = pst[count - 1].pss_idx + 1;
	if ((psp->pss_flags & SW_BLOCK) && (psp->pss_major == major(devno))
	    && (psp->pss_minor == minor(devno))) {
	    match = 1;
	    break;
	}
    }
    return (match);
}

#endif /* AFS_HPUX_ENV */


is_pre_init(rdevnum)
     dev_t rdevnum;
{
    if (rdevnum == -1)
	return (1);
    return (0);
}

is_roroot()
{
#ifndef UID_NO_CHANGE
    struct stat stslash;

    if (stat("/", &stslash) < 0)
	return (0);
    if (chown("/", stslash.st_uid, stslash.st_gid) == 0)
	return (0);
#else
    if (chown("/", UID_NO_CHANGE, GID_NO_CHANGE) == 0)
	return (0);
#endif
    else if (errno != EROFS) {
	printf("fsck: chown failed: %d\n", errno);
	return (0);
    }
    return (1);
}

is_hotroot(fs_device)
     char *fs_device;
{
    struct stat stslash, stblock, stchar;
    char blockdev[BUFSIZ];

    strcpy(blockdev, fs_device);
    if (stat("/", &stslash) < 0) {
	pfatal("Can't stat root\n");
	return (0);
    }

    if (stat(blockdev, &stblock) < 0) {
	pfatal("Can't stat %s\n", blockdev);
	return (0);
    }

    if (is_pre_init(stblock.st_rdev))
	return (0);

    /* convert the device name to be block device name */
    if ((stblock.st_mode & S_IFMT) == S_IFCHR) {
	unrawname(blockdev);
	if (stat(blockdev, &stblock) < 0)
	    return (0);
    }
    if ((stblock.st_mode & S_IFMT) != S_IFBLK)
	return (0);
    if (stslash.st_dev != stblock.st_rdev)
	return (0);
    /* is root file system mounted read only? */
    if (is_roroot())
	return (0);
    return (1);
}

is_root(rdev_num)
     dev_t rdev_num;
{
    struct stat stslash;

    if (stat("/", &stslash) < 0)
	return (0);
    if (stslash.st_dev != rdev_num)
	return (0);
    return (1);
}

getline(fp, loc, maxlen)
     FILE *fp;
     char *loc;
{
    register n;
    register char *p, *lastloc;

    p = loc;
    lastloc = &p[maxlen - 1];
    while ((n = getc(fp)) != '\n') {
	if (n == EOF)
	    return (EOF);
	if (!isspace(n) && p < lastloc)
	    *p++ = n;
    }
    *p = 0;
    return (p - loc);
}

/*
 * freply - prints the string, s, gets a reply fromterminal and returns
 * 0 - no (don't continue), and	1 - yes (continue)
 */
freply(s)
     char *s;
{
    char line[80];

    if (!isatty(0))
	errexit("exiting\n");
    printf("\n%s? ", s);
    if (getline(stdin, line, sizeof(line)) == EOF)
	errexit("\n");
    if (line[0] == 'y' || line[0] == 'Y')
	return (1);
    return (0);
}


#if   defined(AFS_HPUX110_ENV)
/*
 *  Refer to function compare_sblocks() in HP's fsck.c 
 *
 *  DESCRIPTION:
 *     This routine will compare the primary superblock (PRIM_SBLOCK) to the
 *     alternate superblock (ALT_SBLOCK).  This routine will take into account
 *     that certain fields in the alternate superblock could be different than
 *     in the primary superblock.  This routine checks the "static" fields
 *     and ignores the "dynamic" fields.  Superblocks with the same "static"
 *     fields are considered equivalent.
 *
 */
UpdateAlternateSuper(prim_sblock, alt_sblock)
     struct fs *prim_sblock;
     struct fs *alt_sblock;	/* will be modified */
{
    /*
     * Set all possible fields that could differ, then do check
     * of whole super block against an alternate super block.
     *
     * Copy dynamic fields of prim_sblock into alt_sblock
     */
    alt_sblock->fs_time = prim_sblock->fs_time;
    alt_sblock->fs_minfree = prim_sblock->fs_minfree;
    alt_sblock->fs_rotdelay = prim_sblock->fs_rotdelay;
    alt_sblock->fs_maxcontig = prim_sblock->fs_maxcontig;
    alt_sblock->fs_maxbpg = prim_sblock->fs_maxbpg;
    alt_sblock->fs_mirror = prim_sblock->fs_mirror;
    alt_sblock->fs_cstotal = prim_sblock->fs_cstotal;
    alt_sblock->fs_fmod = prim_sblock->fs_fmod;
    alt_sblock->fs_clean = prim_sblock->fs_clean;
    alt_sblock->fs_ronly = prim_sblock->fs_ronly;
    alt_sblock->fs_flags = prim_sblock->fs_flags;
    alt_sblock->fs_cgrotor = prim_sblock->fs_cgrotor;
#ifdef __LP64__
    alt_sblock->fs_csp = prim_sblock->fs_csp;
#else /* not __LP64__ */
    alt_sblock->fs_csp = prim_sblock->fs_csp;
    alt_sblock->fs_csp_pad = prim_sblock->fs_csp_pad;
#endif /* not __LP64__ */
    memcpy((char *)alt_sblock->fs_fsmnt, (char *)prim_sblock->fs_fsmnt,
	   sizeof prim_sblock->fs_fsmnt);
    memcpy((char *)alt_sblock->fs_fname, (char *)prim_sblock->fs_fname,
	   sizeof prim_sblock->fs_fname);
    memcpy((char *)alt_sblock->fs_fpack, (char *)prim_sblock->fs_fpack,
	   sizeof prim_sblock->fs_fpack);
    if (prim_sblock->fs_featurebits & FSF_LARGEUIDS)
	alt_sblock->fs_featurebits |= FSF_LARGEUIDS;
    else
	alt_sblock->fs_featurebits &= ~FSF_LARGEUIDS;
}
#endif /* AFS_HPUX110_ENV */
