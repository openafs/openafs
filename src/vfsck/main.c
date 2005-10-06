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
    ("$Header: /cvs/openafs/src/vfsck/main.c,v 1.8.2.2 2005/10/03 02:46:33 shadow Exp $");

#define VICE			/* allow us to put our changes in at will */
#include <stdio.h>

#include <sys/param.h>
#include <sys/time.h>

#ifdef AFS_SUN_ENV
#define KERNEL
#endif /* AFS_SUN_ENV */
#include <sys/mount.h>
#ifdef AFS_SUN_ENV
#undef KERNEL
#endif

#include <sys/file.h>

#ifdef	AFS_OSF_ENV
#include <sys/vnode.h>
#include <sys/mount.h>
#include <ufs/inode.h>
#include <ufs/fs.h>
#else /* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
#define VFS
#include <sys/vnode.h>
#ifdef	  AFS_SUN5_ENV
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
#ifdef	AFS_SUN5_ENV
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#ifdef	XAFS_SUN_ENV
#include <mntent.h>
#else
#ifdef	AFS_SUN5_ENV
#include <sys/mnttab.h>
#include <sys/mntent.h>
#include <sys/vfstab.h>
#include <sys/ustat.h>
#else
#include <fstab.h>
#endif
#endif
#include "fsck.h"
#include <errno.h>
#include <sys/signal.h>

char *rawname(), *unrawname(), *blockcheck(), *malloc();
void catch(), catchquit(), voidquit();
static int tryForce;
int returntosingle;

extern int errno;

struct part {
    char *name;			/* device name */
    char *fsname;		/* mounted filesystem name */
    struct part *next;		/* forward link of partitions on disk */
} *badlist, **badnext = &badlist;

struct disk {
    char *name;			/* disk base name */
    struct disk *next;		/* forward link for list of disks */
    struct part *part;		/* head of list of partitions on disk */
    int pid;			/* If != 0, pid of proc working on */
} *disks;

int nrun, ndisks, maxrun, wflag = 0;
#ifdef	AFS_HPUX_ENV
int fixed;
#endif

#if	defined(AFS_HPUX100_ENV)
#include <ustat.h>
#include <mntent.h>
#endif

#ifdef VICE
#define	msgprintf   vfscklogprintf
#else /* VICE */
#define	msgprintf   printf
#endif /* VICE */

#ifdef	AFS_SUN5_ENV
int mnt_passno = 0;
#endif

#include "AFS_component_version_number.c"

#ifdef	AFS_HPUX_ENV
int ge_danger = 0;		/* on when fsck is not able to fix the dirty file 
				 * system within single run. Problems like dup table
				 * overflow, maxdup is exceeding MAXDUP.. etc. could
				 * potentailly prevent fsck from doing a complete 
				 * repair. This is found in a GE hotsite. */
#endif

main(argc, argv)
     int argc;
     char *argv[];
{
    struct fstab *fsp;
    int pid, passno, sumstatus;
    char *name;
    register struct disk *dk, *nextdisk;
    register struct part *pt;
    extern char *AFSVersion;	/* generated version */
#ifdef	AFS_SUN5_ENV
    int other_than_ufs = 0;
    char *subopt;
    struct vfstab vt;
    FILE *vfile;
    int ret;
    struct vfstab vget;
    FILE *fd;
#endif

    sync();
    tryForce = 0;
#if	defined(AFS_HPUX_ENV)
    pclean = 0;
#endif
#if	defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV)
    fflag = 0;
#endif
#ifdef	AFS_SUN5_ENV
    fsflag = oflag = mflag = exitstat = 0;
#endif
#if	defined(AFS_HPUX100_ENV)
    mflag = 0;
#endif
    printf("----Open AFS (R) %s fsck----\n", AFSVersion);	/* generated version */
    if (access("/TRYFORCE", 0) == 0)
	tryForce = 1;
    while (--argc > 0 && **++argv == '-') {
	switch (*++*argv) {

#if	defined(AFS_HPUX_ENV)
#if	defined(AFS_HPUX100_ENV)
	case 'f':		/* default yes to answer force to check */
	    fflag++;
	    break;
#else /* AFS_HPUX100_ENV */
#ifdef	AFS_HPUX_ENV
	case 'F':		/* default yes to answer force to check */
	    fflag++;
	    break;
#endif /* AFS_HPUX_ENV */
#endif /* AFS_HPUX100_ENV */
	case 'P':
	    pclean++;
	    preen++;
	    break;
#endif
	case 'p':
	    preen++;
	    break;
#if	defined(AFS_HPUX100_ENV)
	case 'V':
	    {
		int opt_count;
		char *opt_text;

		(void)fprintf(stdout, "fsck -F hfs ");
		for (opt_count = 1; opt_count < argc; opt_count++) {
		    opt_text = argv[opt_count];
		    if (opt_text)
			(void)fprintf(stdout, " %s ", opt_text);
		}
		(void)fprintf(stdout, "\n");
		exit(0);
	    }
	    break;
	case 'm':
	    mflag++;
	    break;
#endif
#ifdef	AFS_SUN5_ENV
	case 'V':
	    {
		int opt_count;
		char *opt_text;

		(void)fprintf(stdout, "fsck -F ufs ");
		for (opt_count = 1; opt_count < argc; opt_count++) {
		    opt_text = argv[opt_count];
		    if (opt_text)
			(void)fprintf(stdout, " %s ", opt_text);
		}
		(void)fprintf(stdout, "\n");
	    }
	    break;

	case 'o':
	    subopt = *++argv;
	    argc--;
	    while (*subopt != '\0') {
		if (*subopt == 'p') {
		    preen++;
		    break;
		} else if (*subopt == 'b') {
		    if (argv[0][1] != '\0') {
			bflag = atoi(argv[0] + 1);
		    } else {
			bflag = atoi(*++argv);
			argc--;
		    }
		    msgprintf("Alternate super block location: %d\n", bflag);
		    break;
		} else if (*subopt == 'd') {
		    debug++;
		    break;
		} else if (*subopt == 'r') {
		    break;
		} else if (*subopt == 'w') {
		    wflag++;
		    break;
		} else if (*subopt == 'c') {
		    cvtflag++;
		    break;
		} else if (*subopt == 'f') {
		    fflag++;
		    break;
		} else {
		    errexit("-o %c option?\n", *subopt);
		}
		subopt++;
		++argv;
		argc--;
	    }
	    oflag++;
	    break;
	case 'm':
	    mflag++;
	    break;
#else
	case 'b':
	    if (argv[0][1] != '\0') {
		bflag = atoi(argv[0] + 1);
	    } else {
		bflag = atoi(*++argv);
		argc--;
	    }
	    msgprintf("Alternate super block location: %d\n", bflag);
	    break;

	case 'c':
	    cvtflag++;
	    break;

	    /* who knows?  defined, but doesn't do much */
	case 'r':
	    break;

	case 'w':		/* check writable only */
	    wflag++;
	    break;
	case 'd':
	    debug++;
	    break;
	case 'l':
	    if (!isdigit(argv[1][0]))
		errexit("-l flag requires a number\n");
	    maxrun = atoi(*++argv);
	    argc--;
	    break;
#if	!defined(AFS_HPUX100_ENV)
	case 'm':
	    if (!isdigit(argv[1][0]))
		errexit("-m flag requires a mode\n");
	    sscanf(*++argv, "%o", &lfmode);
	    if (lfmode & ~07777)
		errexit("bad mode to -m: %o\n", lfmode);
	    argc--;
	    printf("** lost+found creation mode %o\n", lfmode);
	    break;
#endif /* AFS_HPUX100_ENV */
#endif /* AFS_SUN5_ENV */
#ifdef	AFS_OSF_ENV
	case 'o':
	    fflag++;
	    break;
#endif /* AFS_OSF_ENV */
	case 'n':
	case 'N':
	    nflag++;
	    yflag = 0;
	    break;

	    /*
	     * NOTE: -q flag is used only by HPux fsck versions but we add it for all systems since
	     * it's general/useful flag to use.
	     */
	case 'q':
	    qflag++;
	    break;

	case 'y':
	case 'Y':
	    yflag++;
	    nflag = 0;
	    break;

	default:
	    errexit("%c option?\n", **argv);
	}
    }
    /*
     * The following checks were only available on hpux but are useful to all systems.
     */
    if (nflag && preen)
	errexit("Incompatible options: -n and -p\n");
    if (nflag && qflag)
	errexit("Incompatible options: -n and -q\n");

#ifdef	AFS_SUN5_ENV
    rflag++;			/* check raw devices */
#endif
    if (signal(SIGINT, SIG_IGN) != SIG_IGN)
	(void)signal(SIGINT, catch);
    if (preen)
	(void)signal(SIGQUIT, catchquit);
    if (argc) {
	while (argc-- > 0) {
	    hotroot = 0;
#ifdef	AFS_SUN5_ENV
	    if (wflag && !writable(*argv)) {
		(void)fprintf(stderr, "not writeable '%s'\n", *argv);
		argv++;
	    } else
#endif
		checkfilesys(*argv++, NULL);
	}
#ifdef	AFS_HPUX_ENV
	if (ge_danger)
	    exit(-1);
#endif
#ifdef	AFS_SUN5_ENV
	exit(exitstat);
#else
	exit(0);
#endif
    }
#ifndef	AFS_SUN5_ENV
    sumstatus = 0;
#ifdef	AFS_SUN5_ENV
    if (fstype == NULL || strcmp(fstype, MNTTYPE_UFS) == 0) {
	int status;

	if ((fd = fopen(VFSTAB, "r")) == NULL) {
	    errexit("vfsck: cannot open vfstab\n");
	}
	while ((ret = getvfsent(fd, &vget)) == 0) {
	    if (strcmp(vget.vfs_fstype, MNTTYPE_UFS)
		&& numbers(vget.vfs_fsckpass)) {
		other_than_ufs++;
		continue;
	    }
	    if (numbers(vget.vfs_fsckpass))
		passno = atoi(vget.vfs_fsckpass);
	    else
		continue;
	    if (passno < 1)
		continue;
	    if (preen == 0 || passno == 1) {
		checkfilesys(vget.vfs_fsckdev, get.vfs_mountp);
	    } else if (passno > 1) {
		addpart(vget.vfs_fsckdev, vget.vfs_special);
	    }
	}
#else
    for (passno = 1; passno <= 2; passno++) {
	if (setfsent() == 0)
	    errexit("Can't open checklist file: %s\n", FSTAB);
	while ((fsp = getfsent()) != 0) {
	    if (strcmp(fsp->fs_type, FSTAB_RW)
		&& strcmp(fsp->fs_type, FSTAB_RO)
		&& strcmp(fsp->fs_type, FSTAB_RQ))
		continue;
#ifdef	AFS_OSF_ENV
	    if (strcmp(fsp->fs_vfstype, "ufs") || fsp->fs_passno == 0) {
		continue;
	    }
#endif /* AFS_OSF_ENV */
	    if (preen == 0 || passno == 1 && fsp->fs_passno == 1) {
		if (passno == 1) {
		    name = blockcheck(fsp->fs_spec);
		    if (name != NULL) {
			checkfilesys(name, fsp->fs_file);
		    } else if (preen) {
			printf("pid %d exiting 8/1\n", getpid());
			exit(8);
		    }
		}
	    } else if (passno == 2 && fsp->fs_passno > 1) {
		name = blockcheck(fsp->fs_spec);
		if (name == NULL) {
		    pwarn("BAD DISK NAME %s\n", fsp->fs_spec);
		    sumstatus |= 8;
		    printf("pid %d saw bad disk name 8/3\n", getpid());
		    continue;
		}
		addpart(name, fsp->fs_file);
	    }
	}
#endif /* AFS_SUN5_ENV */
    }
    if (preen) {
	int status, rc;

	if (maxrun == 0)
	    maxrun = ndisks;
	if (maxrun > ndisks)
	    maxrun = ndisks;
	nextdisk = disks;
	for (passno = 0; passno < maxrun; ++passno) {
	    startdisk(nextdisk);
	    nextdisk = nextdisk->next;
	}
	while ((pid = wait(&status)) != -1) {
	    for (dk = disks; dk; dk = dk->next)
		if (dk->pid == pid)
		    break;
	    if (dk == 0) {
		printf("Unknown pid %d\n", pid);
		continue;
	    }
	    rc = WEXITSTATUS(status);
	    if (WIFSIGNALED(status)) {
		printf("%s (%s): EXITED WITH SIGNAL %d\n", dk->part->name,
		       dk->part->fsname, WTERMSIG(status));
		rc = 8;
	    }
	    if (rc != 0) {
		sumstatus |= rc;
		*badnext = dk->part;
		badnext = &dk->part->next;
		dk->part = dk->part->next;
		*badnext = NULL;
	    } else
		dk->part = dk->part->next;
	    dk->pid = 0;
	    nrun--;
	    if (dk->part == NULL)
		ndisks--;

	    if (nextdisk == NULL) {
		if (dk->part)
		    startdisk(dk);
	    } else if (nrun < maxrun && nrun < ndisks) {
		for (;;) {
		    if ((nextdisk = nextdisk->next) == NULL)
			nextdisk = disks;
		    if (nextdisk->part != NULL && nextdisk->pid == 0)
			break;
		}
		startdisk(nextdisk);
	    }
	}
    }
    if (sumstatus) {
	if (badlist == 0) {
	    printf("pid %d exiting 8/2\n", getpid());
	    exit(8);
	}
	printf("THE FOLLOWING FILE SYSTEM%s HAD AN %s\n\t",
	       badlist->next ? "S" : "", "UNEXPECTED INCONSISTENCY:");
	for (pt = badlist; pt; pt = pt->next)
	    printf("%s (%s)%s", pt->name, pt->fsname, pt->next ? ", " : "\n");
	exit(8);
    }
#ifdef	AFS_SUN5_ENV
    fclose(fd);
#else
    (void)endfsent();
#endif
    if (returntosingle)
	exit(2);
#endif /* !AFS_SUN5_ENV */
    exit(0);
}

struct disk *
finddisk(name)
     char *name;
{
    register struct disk *dk, **dkp;
    register char *p;
    int len;

    for (p = name + strlen(name) - 1; p >= name; --p)
	if (isdigit(*p)) {
	    len = p - name + 1;
	    break;
	}
    if (p < name)
	len = strlen(name);

    for (dk = disks, dkp = &disks; dk; dkp = &dk->next, dk = dk->next) {
	if (strncmp(dk->name, name, len) == 0 && dk->name[len] == 0)
	    return (dk);
    }
    if ((*dkp = (struct disk *)malloc(sizeof(struct disk))) == NULL)
	errexit("out of memory");
    dk = *dkp;
    if ((dk->name = malloc((unsigned int)len + 1)) == NULL)
	errexit("out of memory");
    strncpy(dk->name, name, len);
    dk->name[len] = '\0';
    dk->part = NULL;
    dk->next = NULL;
    dk->pid = 0;
    ndisks++;
    return (dk);
}

addpart(name, fsname)
     char *name, *fsname;
{
    struct disk *dk = finddisk(name);
    register struct part *pt, **ppt = &dk->part;

    for (pt = dk->part; pt; ppt = &pt->next, pt = pt->next)
	if (strcmp(pt->name, name) == 0) {
	    printf("%s in fstab more than once!\n", name);
	    return;
	}
    if ((*ppt = (struct part *)malloc(sizeof(struct part))) == NULL)
	errexit("out of memory");
    pt = *ppt;
    if ((pt->name = malloc((unsigned int)strlen(name) + 1)) == NULL)
	errexit("out of memory");
    strcpy(pt->name, name);
    if ((pt->fsname = malloc((unsigned int)strlen(fsname) + 1)) == NULL)
	errexit("out of memory");
    strcpy(pt->fsname, fsname);
    pt->next = NULL;
}

startdisk(dk)
     register struct disk *dk;
{

    nrun++;
    dk->pid = fork();
    if (dk->pid < 0) {
	perror("fork");
	exit(8);
    }
    if (dk->pid == 0) {
	(void)signal(SIGQUIT, voidquit);
	checkfilesys(dk->part->name, dk->part->fsname);
	exit(0);
    }
}

checkfilesys(filesys, parname)
     char *filesys;
{
    daddr_t n_ffree, n_bfree;
    struct dups *dp;
    struct stat tstat;		/* for ultrix 3 unmount */
    struct zlncnt *zlnp;
    char devbuffer[128];
    int ret_val;

#ifdef	AFS_OSF_ENV
    int temp;
#endif /* AFS_OSF_ENV */

#ifdef	AFS_SUN_ENV
    iscorrupt = 1;
#endif
#ifdef	AFS_SUN5_ENV
    mountedfs = 0;
    isconvert = 0;
#endif
#ifdef	AFS_HPUX_ENV
    ge_danger = 0;		/* set to 1 by any table overflow or more 
				 * dup/bad blocks than expected */

    fixed = 1;			/* set to 0 by any 'no' reply */
#endif
    strcpy(devbuffer, filesys);	/* copy the file system name to the device buffer */
    devname = devbuffer;	/* remember generic ptr for later */
    EnsureDevice(devname);	/* canonicalize name */
    if (debug && preen)
	pinfo("starting\n");

	ret_val = setup(devname);

	if (ret_val == 0) {
#ifdef	AFS_SUN_ENV
	    if (iscorrupt == 0)
		return;
#endif
	    if (preen)
		pfatal("CAN'T CHECK FILE SYSTEM.");
#ifdef	AFS_SUN5_ENV
	    if ((exitstat == 0) && (mflag))
		exitstat = 32;
	    exit(exitstat);
#endif
	    return (0);
#ifdef	AFS_HPUX_ENV
	} else if (ret_val == -1) {	/* pclean && FS_CLEAN */
	    return (1);
#endif
#if	defined(AFS_OSF_ENV)
	} else if (ret_val == FS_CLEAN) {	/* pclean && FS_CLEAN */
	    return (1);
#endif
	}
#if	defined(AFS_HPUX100_ENV)
	if (mflag)
	    check_sanity(filesys);
#endif

#ifdef	AFS_SUN5_ENV
	if (mflag)
	    check_sanity(filesys);
	if (debug)
	    printclean();
#endif
#ifdef	AFS_SUN_ENV
	iscorrupt = 0;
#endif
	/*
	 * 1: scan inodes tallying blocks used
	 */
	if (preen == 0) {
#if	defined(AFS_SUN5_ENV)
	    if (mountedfs)
		msgprintf("** Currently Mounted on %s\n", sblock.fs_fsmnt);
	    else
#endif
		msgprintf("** Last Mounted on %s\n", sblock.fs_fsmnt);
	    if (hotroot)
		msgprintf("** Root file system\n");
#ifdef	AFS_SUN5_ENV
	    if (mflag) {
		printf("** Phase 1 - Sanity Check only\n");
		return;
	    } else
#endif
		msgprintf("** Phase 1 - Check Blocks and Sizes\n");
	}
	pass1();

	/*
	 * 1b: locate first references to duplicates, if any
	 */
	if (duplist) {
	    if (preen)
		pfatal("INTERNAL ERROR: dups with -p");
	    msgprintf("** Phase 1b - Rescan For More DUPS\n");
	    pass1b();
	}

	/*
	 * 2: traverse directories from root to mark all connected directories
	 */
	if (preen == 0)
	    msgprintf("** Phase 2 - Check Pathnames\n");
	pass2();

	/*
	 * 3: scan inodes looking for disconnected directories
	 */
	if (preen == 0)
	    msgprintf("** Phase 3 - Check Connectivity\n");
	pass3();

	/*
	 * 4: scan inodes looking for disconnected files; check reference counts
	 */
	if (preen == 0)
	    msgprintf("** Phase 4 - Check Reference Counts\n");
	pass4();

	/*
	 * 5: check and repair resource counts in cylinder groups
	 */
	if (preen == 0)
	    msgprintf("** Phase 5 - Check Cyl groups\n");
	pass5();

#if	defined(AFS_SUN_ENV)
    updateclean();
    if (debug)
	printclean();
#endif
    /*
     * print out summary statistics
     */
    n_ffree = sblock.fs_cstotal.cs_nffree;
    n_bfree = sblock.fs_cstotal.cs_nbfree;
#ifdef VICE
#if defined(ACLS) && defined(AFS_HPUX_ENV)
    pinfo("%d files, %d icont, %d used, %d free", n_files, n_cont, n_blks,
	  n_ffree + sblock.fs_frag * n_bfree);
#else
    pinfo("%d files, %d used, %d free", n_files, n_blks,
	  n_ffree + sblock.fs_frag * n_bfree);
#endif
    if (nViceFiles)
	msgprintf(", %d AFS files", nViceFiles);
    msgprintf(" (%d frags, %d blocks, %.1f%% fragmentation)\n", n_ffree,
	      n_bfree, (float)(n_ffree * 100) / sblock.fs_dsize);
#else /* VICE */
#if defined(ACLS) && defined(AFS_HPUX_ENV)
    pinfo("%d files, %d icont, %d used, %d free ", n_files, n_cont, n_blks,
	  n_ffree + sblock.fs_frag * n_bfree);
#else
    pinfo("%d files, %d used, %d free ", n_files, n_blks,
	  n_ffree + sblock.fs_frag * n_bfree);
#endif
    n printf("(%d frags, %d blocks, %.1f%% fragmentation)\n", n_ffree,
	     n_bfree, (float)(n_ffree * 100) / sblock.fs_dsize);
#endif /* VICE */
    if (debug && (n_files -= maxino - ROOTINO - sblock.fs_cstotal.cs_nifree))
	msgprintf("%d files missing\n", n_files);
    if (debug) {
	n_blks += sblock.fs_ncg * (cgdmin(&sblock, 0) - cgsblock(&sblock, 0));
	n_blks += cgsblock(&sblock, 0) - cgbase(&sblock, 0);
	n_blks += howmany(sblock.fs_cssize, sblock.fs_fsize);
	if (n_blks -= maxfsblock - (n_ffree + sblock.fs_frag * n_bfree))
	    printf("%d blocks missing\n", n_blks);
	if (duplist != NULL) {
	    msgprintf("The following duplicate blocks remain:");
	    for (dp = duplist; dp; dp = dp->next)
		msgprintf(" %d,", dp->dup);
	    msgprintf("\n");
	}
	if (zlnhead != NULL) {
	    msgprintf("The following zero link count inodes remain:");
	    for (zlnp = zlnhead; zlnp; zlnp = zlnp->next)
		msgprintf(" %d,", zlnp->zlncnt);
	    msgprintf("\n");
	}
    }
#ifdef	AFS_HPUX_ENV
    /* if user's specification denotes that the file system block
     * is going to be modified (nflag == 0) then fsck store the
     * correct magic number in the super block if it is not already
     * there
     */
    if (!nflag && !(fswritefd < 0)) {
	if (ge_danger) {
	    printf("***** FILE SYSTEM IS NOT CLEAN, FSCK AGAIN *****\n");
	    fsmodified++;
	} else {
	    if (!hotroot) {
		if (fixed && (sblock.fs_clean != FS_CLEAN)) {
		    if (!preen && !qflag)
			printf("***** MARKING FILE SYSTEM CLEAN *****\n");
		    sblock.fs_clean = FS_CLEAN;
		    fsmodified++;
		}
	    } else {
		/* fix FS_CLEAN if changes made and no 'no' replies */
		if (fsmodified && fixed)
		    sblock.fs_clean = FS_CLEAN;
		/*
		 *  Fix fs_clean if there were no 'no' replies.
		 *  This is done for both the s300 and s800.  The s800 root will be 
		 *  guaranteed clean as of 7.0.
		 */
		if (fixed && (sblock.fs_clean != FS_OK)) {
		    if (!preen && !qflag)
			printf("***** MARKING FILE SYSTEM CLEAN *****\n");
		    sblock.fs_clean = FS_CLEAN;
		    fsmodified++;
		}
	    }
	}
    }
#endif
    zlnhead = NULL;
    duplist = NULL;

#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN3_ENV)	/* WAS AFS_SUN5_ENV */
#ifdef	notdef
    inocleanup();
#endif
    if (fsmodified)
	fixstate = 1;
    else
	fixstate = 0;
    if (hotroot && sblock.fs_clean == FSACTIVE)
	rebflg = 1;
#ifdef	AFS_SUN5_ENV
    else if (!((sblock.fs_state + (afs_int32) sblock.fs_time == FSOKAY) &&
#else
    else if (!((fs_get_state(&sblock) + (afs_int32) sblock.fs_time == FSOKAY)
	       &&
#endif
	       (sblock.fs_clean == FSCLEAN || sblock.fs_clean == FSSTABLE))) {
	if (yflag || !iscorrupt) {
	    printf("%s FILE SYSTEM STATE SET TO OKAY\n", devname);
	    fixstate = 1;
	} else {
	    printf("%s FILE SYSTEM STATE NOT SET TO OKAY\n", devname);
	    fixstate = 0;
	}
    }
    if (fixstate) {
	(void)time(&sblock.fs_time);
	if (!iscorrupt) {
	    if (hotroot && rebflg)
		sblock.fs_clean = FSACTIVE;
	    else
		sblock.fs_clean = FSSTABLE;
#ifdef	AFS_SUN5_ENV
	    sblock.fs_state = FSOKAY - (afs_int32) sblock.fs_time;
#else
	    fs_set_state(&sblock, FSOKAY - (afs_int32) sblock.fs_time);
#endif
	}
	sbdirty();
    }
#else
#ifdef	AFS_OSF_ENV
    if (!nflag && !bflag && !hotroot) {
	temp = fsmodified;
	sblock.fs_clean = FS_CLEAN;
	(void)time(&sblock.fs_time);
	sbdirty();
	flush(fswritefd, &sblk);
	fsmodified = temp;
    }
#else /* AFS_OSF_ENV */
    if (fsmodified) {
	(void)time(&sblock.fs_time);
	sbdirty();
    }
#endif
#endif
    ckfini();
    free(blockmap);
    free(statemap);
    free((char *)lncntp);
    lncntp = NULL;
    blockmap = statemap = NULL;
#ifdef	AFS_SUN5_ENV
    if (iscorrupt)
	exitstat = 36;
#endif
    if (!fsmodified)
	return;
    if (!preen) {
	msgprintf("\n***** FILE SYSTEM WAS MODIFIED *****\n");

	if (hotroot)
	    msgprintf("\n***** REBOOT UNIX *****\n");
    }
#ifdef	AFS_SUN5_ENV
    if (mountedfs || hotroot) {
	exitstat = 40;
    }
#endif
    if (hotroot) {
	sync();
#ifdef	AFS_HPUX_ENV
	if (ge_danger)
	    exit(-1);
	else
#endif
#ifdef  AFS_SUN5_ENV
	    exit(exitstat);
#else
	    exit(4);
#endif
    }
#ifdef VICE
    (void)close(fsreadfd);
    (void)close(fswritefd);
    if (nViceFiles || tryForce) {
	/* Modified file system with vice files: force full salvage */
	/* Salvager recognizes the file FORCESALVAGE in the root of each partition */
	struct ufs_args ufsargs;

	char pname[100], fname[100], *special;
	int fd, code, failed = 0;

	msgprintf
	    ("%s: AFS file system partition was modified; forcing full salvage\n",
	     devname);
	devname = unrawname(devname);
	special = (char *)strrchr(devname, '/');
	if (!special++)
	    special = devname;
	strcpy(pname, "/etc/vfsck.");	/* Using /etc, rather than /tmp, since
					 * /tmp is a link to /usr/tmp on some systems, and isn't mounted now */
	strcat(pname, special);
#ifdef AFS_SUN_ENV
	/* if system mounted / as read-only, we'll try to fix now */
	if (access("/", W_OK) < 0 && errno == EROFS) {
	    code = system("mount -o remount /");
	    if (code) {
		printf("Couldn't remount / R/W; continuing anyway (%d).\n",
		       errno);
		failed = 1;
	    }
	}
#endif
#ifdef	AFS_OSF_ENV
	/* if system mounted / as read-only, we'll try to fix now */
	if (access("/", W_OK) < 0 && errno == EROFS) {
	    printf("Can't RW acceess /; %d\n", errno);
	    code = system("/sbin/mount -u /");
	    if (code) {
		printf("Couldn't remount / R/W; continuing anyway (%d).\n",
		       errno);
		failed = 1;
	    }
	}
#endif
	rmdir(pname);
	unlink(pname);
	if (mkdir(pname, 0777) < 0) {
	    if (errno != EEXIST) {
		perror("fsck mkdir");
		failed = 1;
	    }
	}
	if (failed && parname) {
	    strcpy(pname, parname);
	}
#if !defined(AFS_HPUX_ENV)
#ifdef	AFS_SUN5_ENV
	ufsargs.flags = UFSMNT_NOINTR;
#else
	ufsargs.fspec = devname;
#endif
#ifdef	AFS_SUN5_ENV
	if (mount
	    (devname, pname, MS_DATA, "ufs", (char *)&ufsargs,
	     sizeof(ufsargs)) < 0) {
#else
	if (mount(MOUNT_UFS, pname, 0, &ufsargs) < 0) {
#endif
#else
	if (mount(devname, pname, 0) < 0) {
#endif
	    printf
		("Couldn't mount %s on %s to force FULL SALVAGE; continuing anyway (%d)!\n",
		 devname, pname, errno);
	} else {
	    strcpy(fname, pname);
	    strcat(fname, "/FORCESALVAGE");
	    fd = open(fname, O_CREAT, 0);
	    if (fd == -1) {
		errexit("Couldn't create %s to force full salvage!\n", fname);
	    } else {
		fstat(fd, &tstat);
		close(fd);
	    }
#if !defined(AFS_HPUX_ENV) && !defined(AFS_SUN5_ENV) && !defined(AFS_OSF_ENV)
	    unmount(pname);
#else
#if	defined(AFS_OSF_ENV)
	    umount(pname, MNT_NOFORCE);
#else /* AFS_OSF_ENV */
	    umount(devname);
#endif
#endif
	}
	rmdir(pname);
    }
    if (logfile) {
	fsync(fileno(logfile));	/* Since update isn't running */
	fclose(logfile);
	logfile = 0;
    }
#endif /* VICE */

}

char *
blockcheck(name)
     char *name;
{
    struct stat stslash, stblock, stchar;
    char *raw;
    int retried = 0;

    hotroot = 0;
    if (stat("/", &stslash) < 0) {
	perror("/");
	printf("Can't stat root\n");
	return (0);
    }
  retry:
    if (stat(name, &stblock) < 0) {
	perror(name);
	printf("Can't stat %s\n", name);
	return (0);
    }
    if ((stblock.st_mode & S_IFMT) == S_IFBLK) {
	if (stslash.st_dev == stblock.st_rdev) {
	    hotroot++;
#if	!defined(AFS_OSF_ENV)	/*  OSF/1 always uses the raw device, even for / */
	    return (name);
#endif /* AFS_OSF_ENV */
	}
	raw = rawname(name);
	if (raw) {
	    return (raw);
	} else {
	    printf("Cannot find character device for %s\n", name);
	    return (name);
	}
    } else if ((stblock.st_mode & S_IFMT) == S_IFCHR && !retried) {
	name = unrawname(name);
	retried++;
	goto retry;
    }
    printf("Can't make sense out of name %s\n", name);
    return (0);
}


#if	defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV)

#ifdef	AFS_SUN5_ENV
/*
 * exit 0 - file system is unmounted and okay
 * exit 32 - file system is unmounted and needs checking
 * exit 33 - file system is mounted for root file system
 * exit 34 - cannot stat device
 */
check_sanity(filename)
     char *filename;
{
    struct stat stbd, stbr;
    struct ustat usb;
    char *devname;
    struct vfstab vfsbuf;
    FILE *vfstab;
    int is_root = 0;
    int is_usr = 0;
    int is_block = 0;

    if (stat(filename, &stbd) < 0) {
	fprintf(stderr, "ufs fsck: sanity check failed : cannot stat %s\n",
		filename);
	exit(34);
    }

    if ((stbd.st_mode & S_IFMT) == S_IFBLK)
	is_block = 1;
    else if ((stbd.st_mode & S_IFMT) == S_IFCHR)
	is_block = 0;
    else {
	fprintf(stderr,
		"ufs fsck: sanity check failed: %s not block or character device\n",
		filename);
	exit(34);
    }
    /*
     * Determine if this is the root file system via vfstab. Give up
     * silently on failures. The whole point of this is not to care
     * if the root file system is already mounted.
     *
     * XXX - similar for /usr. This should be fixed to simply return
     * a new code indicating, mounted and needs to be checked.
     */
    if ((vfstab = fopen(VFSTAB, "r")) != 0) {
	if (getvfsfile(vfstab, &vfsbuf, "/") == 0) {
	    if (is_block)
		devname = vfsbuf.vfs_special;
	    else
		devname = vfsbuf.vfs_fsckdev;
	    if (stat(devname, &stbr) == 0)
		if (stbr.st_rdev == stbd.st_rdev)
		    is_root = 1;
	}
	if (getvfsfile(vfstab, &vfsbuf, "/usr") == 0) {
	    if (is_block)
		devname = vfsbuf.vfs_special;
	    else
		devname = vfsbuf.vfs_fsckdev;
	    if (stat(devname, &stbr) == 0)
		if (stbr.st_rdev == stbd.st_rdev)
		    is_usr = 1;
	}
    }

    /* XXX - only works if filename is a block device or if
     * character and block device has the same dev_t value */
    if (is_root == 0 && is_usr == 0 && ustat(stbd.st_rdev, &usb) == 0) {
	fprintf(stderr, "ufs fsck: sanity check: %s already mounted\n",
		filename);
	exit(33);
    }
    /*
     * We mount the ufs root file system read-only first.  After fsck
     * runs, we remount the root as read-write.  Therefore, we no longer
     * check for different values for fs_state between the root file 
     * system and the rest of file systems.
     */
    if (!((sblock.fs_state + (time_t) sblock.fs_time == FSOKAY)
	  && (sblock.fs_clean == FSCLEAN || sblock.fs_clean == FSSTABLE))) {
	fprintf(stderr, "ufs fsck: sanity check: %s needs checking\n",
		filename);
	exit(32);
    }
    fprintf(stderr, "ufs fsck: sanity check: %s okay\n", filename);
    exit(0);
}
#endif

#if	defined(AFS_HPUX100_ENV)
check_sanity(filename)
     char *filename;
{
    struct stat stbd, stbr;
    struct ustat usb;
    char *devname;
    FILE *vfstab;
    struct mntent *mnt;
    int is_root = 0;
    int is_usr = 0;
    int is_block = 0;

    if (stat(filename, &stbd) < 0) {
	fprintf(stderr, "hfs fsck: sanity check failed : cannot stat %s\n",
		filename);
	exit(34);
    }

    if ((stbd.st_mode & S_IFMT) == S_IFBLK)
	is_block = 1;
    else if ((stbd.st_mode & S_IFMT) == S_IFCHR)
	is_block = 0;
    else {
	fprintf(stderr,
		"hfs fsck: sanity check failed: %s not block or character device\n",
		filename);
	exit(34);
    }
    /*
     * Determine if this is the root file system via vfstab. Give up
     * silently on failures. The whole point of this is not to care
     * if the root file system is already mounted.
     *
     * XXX - similar for /usr. This should be fixed to simply return
     * a new code indicating, mounted and needs to be checked.
     */
    if ((vfstab = setmntent(FSTAB, "r")) != 0) {
	while (mnt = getmntent(vfstab)) {
	    if (!strcmp(mnt->mnt_dir, "/"))
		if (stat(mnt->mnt_fsname, &stbr) == 0)
		    if (stbr.st_rdev == stbd.st_rdev)
			is_root = 1;

	    if (!strcmp(mnt->mnt_dir, "/usr"))
		if (stat(mnt->mnt_fsname, &stbr) == 0)
		    if (stbr.st_rdev == stbd.st_rdev)
			is_usr = 1;
	}
	endmntent(vfstab);
    }

    /* XXX - only works if filename is a block device or if
     * character and block device has the same dev_t value */
    if (is_root == 0 && is_usr == 0 && ustat(stbd.st_rdev, &usb) == 0) {
	fprintf(stderr, "hfs fsck: sanity check: %s already mounted\n",
		filename);
	exit(33);
    }
    /*
     * We mount the ufs root file system read-only first.  After fsck
     * runs, we remount the root as read-write.  Therefore, we no longer
     * check for different values for fs_state between the root file 
     * system and the rest of file systems.
     */
    if (!((sblock.fs_clean == FS_CLEAN || sblock.fs_clean == FS_OK))) {
	fprintf(stderr, "hfs fsck: sanity check: %s needs checking\n",
		filename);
	exit(32);
    }
    fprintf(stderr, "hfs fsck: sanity check: %s okay\n", filename);
    exit(0);
}
#endif
/* see if all numbers */
numbers(yp)
     char *yp;
{
    if (yp == NULL)
	return (0);
    while ('0' <= *yp && *yp <= '9')
	yp++;
    if (*yp)
	return (0);
    return (1);
}
#endif

/* Convert a raw device name into a block device name. 
 * If the block device is not found, return the raw device name.
 * For HP and SUN, the returned value is not changed. For other 
 * platforms it is changed (I see no rhyme or reason -jpm).
 */
char *
unrawname(rawdev)
     char *rawdev;
{
    static char bldev[256];
    struct stat statbuf;
    int code, i;

    code = stat(rawdev, &statbuf);
    if ((code < 0) || !S_ISCHR(statbuf.st_mode))
	return (rawdev);	/* Not a char device */

    for (i = strlen(rawdev) - 2; i >= 0; i--) {
	if ((rawdev[i] == '/') && (rawdev[i + 1] == 'r')) {
	    strcpy(bldev, rawdev);
	    bldev[i + 1] = 0;
	    strcat(bldev, &rawdev[i + 2]);

	    code = stat(bldev, &statbuf);	/* test for block device */
	    if (!code && S_ISBLK(statbuf.st_mode)) {
#if defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV)
		return (bldev);
#else
		strcpy(rawdev, bldev);	/* Replace */
		return (rawdev);
#endif
	    }
	}
    }
    return (rawdev);
}

/* Convert a block device name into a raw device name. 
 * If the block device is not found, return null
 */
char *
rawname(bldev)
     char *bldev;
{
    static char rawdev[256];
    struct stat statbuf;
    int code, i;

    for (i = strlen(bldev) - 1; i >= 0; i--) {
	if (bldev[i] == '/') {
	    strcpy(rawdev, bldev);
	    rawdev[i + 1] = 'r';
	    rawdev[i + 2] = 0;
	    strcat(rawdev, &bldev[i + 1]);

	    code = stat(rawdev, &statbuf);
	    if (!code && S_ISCHR(statbuf.st_mode))
		return (rawdev);
	}
    }
    return NULL;
}
