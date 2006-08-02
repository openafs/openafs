/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#if	defined(AFS_DUX40_ENV) || (defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)) || defined(AFS_SUN54_ENV)
#if defined AFS_DUX40_ENV
#define OSVERS "DUX 4.0D"
#else
#if	defined(AFS_SUN54_ENV)
#define OSVERS "SunOS 5.6"
#else
#define OSVERS "SunOS 4.1.1"
#endif
#endif
#include <sys/types.h>
#include <rx/xdr.h>
#include <afs/afsint.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef	AFS_SUN5_ENV
#include <sys/fs/ufs_fs.h>
#else
#include <ufs/fs.h>
#endif
#include <sys/stat.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/file.h>
#ifndef ITIMER_REAL
#include <sys/time.h>
#endif /* ITIMER_REAL */
#include <time.h>
#ifdef AFS_VFSINCL_ENV
#include <sys/vnode.h>
#ifdef	AFS_SUN5_ENV
#include <sys/fs/ufs_inode.h>
#else
#include <ufs/inode.h>
#endif
#else /* AFS_VFSINCL_ENV */
#ifdef	AFS_DUX40_ENV
#include <ufs/inode.h>
#else
#include <sys/inode.h>
#endif
#endif /* AFS_VFSINCL_ENV */
#ifdef	AFS_AIX_ENV
#include <sys/vfs.h>
#include <sys/lockf.h>
#else
#ifdef	AFS_HPUX_ENV
#include <unistd.h>
#include <checklist.h>
#else
#ifdef	  AFS_SUN5_ENV
#include <sys/mnttab.h>
#include <sys/mntent.h>
#include <sys/vfstab.h>
#else
#include <fstab.h>
#endif
#endif
#endif
#include <fcntl.h>
#include <afs/osi_inode.h>
#include <errno.h>

#include <nfs.h>
#include <partition.h>
#include <afs/cmd.h>
#include <stdlib.h>
#include <unistd.h>

int icount = 0, iarraysize = 0, *iarray;

char *rawname(), *unrawname(), *vol_DevName(), *blockcheck();
#define ROOTINODE	2
int force = 0, verbose = 0, unconv = 0;

#ifdef	AFS_DUX40_ENV
int
sortinodes(a, b)
     int *a, *b;
{
    if (*a < *b)
	return -1;
    if (*a > *b)
	return +1;
    return 0;
}

int
BuildInodes(path)
     char *path;
{
    DIR *d;
    struct dirent *de;
    char p[256];
    struct stat stats;
    int code = 0;

    d = opendir(path);
    if (!d) {
	printf("Cannot open %s: error %d\n", path, errno);
	return -1;
    }

    while (de = readdir(d)) {
	if (iarraysize == 0) {
	    iarraysize = 1000;
	    iarray = (int *)malloc(iarraysize * sizeof(int));
	}
	if (icount >= iarraysize) {
	    iarraysize += 1000;
	    iarray = (int *)realloc(iarray, iarraysize * sizeof(int));
	}
	iarray[icount] = de->d_ino;
	icount++;

	if ((strcmp(de->d_name, ".") == 0) || (strcmp(de->d_name, "..") == 0))
	    continue;

	strcpy(p, path);
	strcat(p, "/");
	strcat(p, de->d_name);

	code = stat(p, &stats);
	if (code) {
	    printf("Cannot stat %s: error %d\n", path, errno);
	    return -1;
	}
	if (S_ISDIR(stats.st_mode)) {
	    code = BuildInodes(p);
	    if (code)
		break;
	}
    }

    closedir(d);
    return code;
}
#endif

static
ConvCmd(as)
     struct cmd_syndesc *as;
{
    unconv = 0;
    handleit(as);
}

static
UnConvCmd(as)
     struct cmd_syndesc *as;
{
    unconv = 1;
    handleit(as);
}

#if	defined(AFS_SUN54_ENV)

static
handleit(as)
     struct cmd_syndesc *as;
{
    register struct cmd_item *ti;
    char *dname;
    afs_int32 haspart = 0, hasDevice = 0;
    struct vfstab mnt;

    if (as->parms[1].items)
	verbose = 1;
    else
	verbose = 0;
    if (as->parms[2].items)
	force = 1;
    else
	force = 0;

    if (unconv) {
	printf
	    ("Unconverts from a %s AFS partition to a pre-%s compatible format.\n\n",
	     OSVERS, OSVERS);
    } else {
	printf
	    ("Converts a pre-%s AFS partition to a %s compatible format.\n\n",
	     OSVERS, OSVERS);
    }
    if (!force) {
	printf
	    ("*** Must use the '-force' option for command to have effect! ***\n");
	if (!verbose) {
	    printf
		("*** Use the '-verbose' option to report what will be converted ***\n");
	    exit(1);
	}
    }

    for (ti = as->parms[0].items; ti; ti = ti->next) {
	FILE *fsent;
	char *namep = ti->data;
	int found = 0;

	haspart = 1;
	if ((fsent = fopen(VFSTAB, "r")) == NULL) {
	    printf("Unable to open %s ( errno = %d)\n", VFSTAB, errno);
	    exit(2);
	}
	while (!getvfsent(fsent, &mnt)) {
	    char *part = mnt.vfs_mountp;

	    if (!part)
		continue;

	    if (!strncmp(namep, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE)) {
		if (!strncmp(part, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE)) {
		    if (!strncmp(part, namep, strlen(part) + 1)) {
			if (dname = mnt.vfs_fsckdev) {
			    printf("ProcessFileSys %s %s\n", dname, namep);
			    ProcessFileSys(dname, namep);
			    found = 1;
			    break;
			}
		    }
		}
	    }
	}
	if (!found)
	    printf("Unknown input AFS partition name or device: %s\n", namep);
	fclose(fsent);
    }

    /* if raw devices are specified */
    for (ti = as->parms[3].items; ti; ti = ti->next) {
	char *namep = ti->data;

	hasDevice = 1;
	if (namep) {
	    if (!CheckMountedDevice(namep)) {
		printf("Device %s may be mounted, aborting...\n", namep);
		continue;
	    }
	    ProcessFileSys(namep, namep);
	}
    }

    if (!haspart && !hasDevice) {
	int didSome = 0;
	FILE *fsent;

	if ((fsent = fopen(VFSTAB, "r")) == NULL) {
	    printf("Unable to open %s ( errno = %d)\n", VFSTAB, errno);
	    exit(2);
	}
	while (!getvfsent(fsent, &mnt)) {
	    char *part = mnt.vfs_mountp;

	    if (!part)
		continue;

	    if (strncmp(part, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE) == 0) {
		if (dname = mnt.vfs_fsckdev)
		    ProcessFileSys(dname, part);
		didSome++;
	    }
	}
	fclose(fsent);
	if (!didSome)
	    printf
		("No file system partitions named %s* found; not processed\n",
		 VICE_PARTITION_PREFIX);
    }
}

#else /* AFS_SUN54_ENV */

handleit(as)
     struct cmd_syndesc *as;
{
    register struct cmd_item *ti;
    char *dname;
    afs_int32 haspart = 0;

    if (as->parms[1].items)
	verbose = 1;
    else
	verbose = 0;
    if (as->parms[2].items)
	force = 1;
    else
	force = 0;

    if (unconv) {
	printf
	    ("Unconverts from a %s AFS partition to a pre-%s compatible format.\n\n",
	     OSVERS, OSVERS);
    } else {
	printf
	    ("Converts a pre-%s AFS partition to a %s compatible format.\n\n",
	     OSVERS, OSVERS);
    }
    if (!force) {
	printf
	    ("*** Must use the '-force' option for command to have effect! ***\n");
	if (!verbose) {
	    printf
		("*** Use the '-verbose' option to report what will be converted ***\n");
	    exit(1);
	}
    }

    for (ti = as->parms[0].items; ti; ti = ti->next) {
	struct fstab *fsent;
	char *namep = ti->data;
	int found = 0;

	haspart = 1;
	setfsent();
	while (fsent = getfsent()) {
	    char *part = fsent->fs_file;

	    if (!strncmp(namep, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE)) {
		if (!strncmp(part, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE)) {
		    if (!strncmp(part, namep, strlen(part) + 1)) {
			if (dname = unrawname(fsent->fs_spec)) {
			    ProcessFileSys(dname, namep);
			    found = 1;
			    break;
			}
		    }
		}
	    } else {
		if (!rawname(namep) || !unrawname(namep))
		    break;
		if (!strcmp(fsent->fs_spec, rawname(namep))
		    || !strcmp(fsent->fs_spec, unrawname(namep))) {
		    if (dname = unrawname(fsent->fs_spec)) {
			ProcessFileSys(dname, fsent->fs_file);
			found = 1;
			break;
		    }
		}
	    }
	}
	if (!found)
	    printf("Unknown input AFS partition name or device: %s\n", namep);
	endfsent();
    }

    if (!haspart) {
	int didSome = 0;
	struct fstab *fsent;

	setfsent();
	while (fsent = getfsent()) {
	    char *part = fsent->fs_file;

	    if (strncmp(part, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE) == 0) {
		if (dname = unrawname(fsent->fs_spec)) {
		    ProcessFileSys(dname, part);
		    didSome++;
		}
	    }
	}
	endfsent();
	if (!didSome)
	    printf
		("No file system partitions named %s* found; not processed\n",
		 VICE_PARTITION_PREFIX);
    }
}
#endif /* AFS_SUN54_ENV */


#include "AFS_component_version_number.c"

main(argc, argv)
     char **argv;
{
    register struct cmd_syndesc *ts;
    afs_int32 code;

    if (geteuid() != 0) {
	printf("must be run as root; sorry\n");
	exit(1);
    }
#ifdef	AFS_DUX40_ENV
    ts = cmd_CreateSyntax("convert", ConvCmd, 0,
			  "Convert to DUX 4.0D format");
#else
#if	defined(AFS_SUN54_ENV)
    ts = cmd_CreateSyntax("convert", ConvCmd, 0,
			  "Convert to AFS SunOS 5.6 format");
#else
    ts = cmd_CreateSyntax("convert", ConvCmd, 0,
			  "Convert to AFS 4.1.1 format");
#endif
#endif
    cmd_AddParm(ts, "-part", CMD_LIST, CMD_OPTIONAL, "AFS partition name");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose mode");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL,
		"Safeguard enforce switch");
    cmd_AddParm(ts, "-device", CMD_LIST, CMD_OPTIONAL, "AFS raw device name");
#ifdef	AFS_DUX40_ENV
    ts = cmd_CreateSyntax("unconvert", UnConvCmd, 0,
			  "Convert back from OSF 4.0D to earlier formats");
#else
#if	defined(AFS_SUN54_ENV)
    ts = cmd_CreateSyntax("unconvert", UnConvCmd, 0,
			  "Convert back from AFS SunOS 5.6 to earlier formats");
#else
    ts = cmd_CreateSyntax("unconvert", UnConvCmd, 0,
			  "Convert back from AFS 4.1.1 to earlier formats");
#endif
#endif
    cmd_AddParm(ts, "-part", CMD_LIST, CMD_OPTIONAL, "AFS partition name");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose mode");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL,
		"Safeguard enforce switch");
    cmd_AddParm(ts, "-device", CMD_LIST, CMD_OPTIONAL, "AFS raw device name");

    code = cmd_Dispatch(argc, argv);
    exit(code);
}


ProcessFileSys(dname, path)
     char *dname, *path;
{
    struct stat status, stat1;
#if	!defined(AFS_SUN54_ENV)
    struct ufs_args ufsargs;
#endif
    char devbuffer[120], *devname;
    int i, j, mounted = 0, code;

    if (stat(path, &status) == -1) {
	printf("Couldn't find file system \"%s\"\n", path);
	return;
    }
    if (status.st_ino == ROOTINODE) {
	if (force) {
	    printf
		("Partition %s is mounted; only unmounted partitions are processed\n",
		 path);
	    return;
	}
	mounted = 1;
    }
    strcpy(devbuffer, dname);
    devname = devbuffer;
#if	!defined(AFS_SUN54_ENV)
    EnsureDevice(devname);
#endif
    if (!mounted) {
#ifdef	AFS_DUX40_ENV
	ufsargs.fspec = devname;
	code = mount(MOUNT_UFS, path, 0, &ufsargs);
#else
#if	defined(AFS_SUN54_ENV)
	code = 0;
#else
	ufsargs.fspec = devname;
	code = mount("4.2", path, M_NEWTYPE, &ufsargs);
#endif
#endif
	if (code < 0) {
	    printf("Couldn't mount %s on %s (err=%d)\n", devname, path,
		   errno);
	    return;
	}
    }

    if (stat(path, &status) == -1) {
	printf("Couldn't find file system \"%s\"\n", path);
	return;
    }
#ifdef	AFS_DUX40_ENV
    icount = 0;
    code = BuildInodes(path);
    if (code) {
	printf("Couldn't generate list of all inodes in directory %s\n",
	       path);
	return;
    }
    if (icount)
	qsort(iarray, icount, sizeof(int), sortinodes);
#endif

#if	!defined(AFS_SUN54_ENV)
    if (!(devname = vol_DevName(status.st_dev))) {
	printf("Couldn't get devname for %d\n", status.st_dev);
	return;
    }
#endif

    if (ProcessAfsInodes(devname, path) == -1)
	printf("Unable to get inodes for \"%s\"; not processed\n", path);
    if (!mounted) {
#ifdef	AFS_DUX40_ENV
	umount(path);
#endif
    }
}


int
ProcessAfsInodes(devname, partition)
     char *devname, *partition;
{
    union {
	struct fs fs;
	char block[SBSIZE];
    } super;
    int pfd, i, c, e, bufsize, cnt = 0, mod = 0, wcnt = 0, ccnt = 0, ncnt;
    FILE *inodeFile = NULL;
    char rdev[25];
    struct dinode *inodes = NULL, *einodes, *p;
    int code, istep = 0;

    sync();
    sleep(5);			/* XXXX */
#if	defined(AFS_SUN54_ENV)
    sprintf(rdev, "%s", devname);
#else
    sprintf(rdev, "/dev/r%s", devname);
#endif
    pfd = open(rdev, O_RDWR);
    if (pfd < 0) {
	printf("Could not read device %s to get inode list\n", rdev);
	printf("errno %d: %s\n", errno, strerror(errno));
	return -1;
    }
    if (bread(pfd, super.block, SBLOCK, SBSIZE) == -1) {
	printf("Unable to read superblock, partition %s\n", partition);
	goto out;
    }

    /*
     * run a few consistency checks of the superblock
     * (Cribbed from vfsck)
     */
    if ((super.fs.fs_magic != FS_MAGIC) || (super.fs.fs_ncg < 1)
	|| (super.fs.fs_cpg < 1)
	|| (super.fs.fs_ncg * super.fs.fs_cpg < super.fs.fs_ncyl
	    || (super.fs.fs_ncg - 1) * super.fs.fs_cpg >= super.fs.fs_ncyl)
	|| (super.fs.fs_sbsize > SBSIZE)) {
	printf
	    ("There's something wrong with the superblock for partition %s; run vfsck\n",
	     partition);
	goto out;
    }
    bufsize = super.fs.fs_ipg * sizeof(struct dinode);
    if (!(inodes = (struct dinode *)malloc(bufsize))) {
	printf("Unable to allocate enough memory to scan inodes; help!\n");
	goto out;
    }
    einodes = (struct dinode *)(((char *)inodes) + bufsize);
    printf("Processing all AFS inodes on partition %s (device %s)...\n",
	   partition, rdev);
    for (c = 0; c < super.fs.fs_ncg; c++) {
	daddr_t dblk1;
#if	defined(AFS_SUN54_ENV)
	daddr_t f1;
	offset_t off;
#endif

	i = c * super.fs.fs_ipg;
	e = i + super.fs.fs_ipg;
#ifdef	AFS_DUX40_ENV
	dblk1 = fsbtodb(&super.fs, itod(&super.fs, i));
	code = lseek(pfd, (off_t) ((off_t) dblk1 * DEV_BSIZE), L_SET);
#else
#if	defined(AFS_SUN54_ENV)
	f1 = fsbtodb(&super.fs, itod(&super.fs, i));
	off = (offset_t) f1 << DEV_BSHIFT;
	code = llseek(pfd, off, L_SET);
#else
	code =
	    lseek(pfd, dbtob(fsbtodb(&super.fs, itod(&super.fs, i))), L_SET);
#endif
#endif
	if (code == -1) {
	    printf("Error reading inodes for partition %s; run vfsck\n",
		   partition);
	    printf("%d: %s\n", errno, strerror(errno));
#if	defined(AFS_SUN54_ENV)
	    printf("file number = %d; offset = %ld\n", pfd, off);
#else
	    printf("file number = %d\n", pfd);
#endif
	    goto out;
	}
	while (i < e) {
	    wcnt = ncnt = mod = 0;
	    code = read(pfd, inodes, bufsize);
	    if (code != bufsize) {
		printf("Error reading inodes for partition %s; run vfsck\n",
		       partition);
		if (code < 0) {
		    printf("errno %d: %s\n", errno, strerror(errno));
		}
		goto out;
	    }

	    /* Step through each inode */
	    for (p = inodes; p < einodes && i < e; i++, p++)
#ifdef	AFS_DUX40_ENV
	    {
		afs_uint32 v1, v2, v3, v4;
		afs_uint32 volid, vnode, uniq, vers;
		afs_uint32 type, rwvol;
		afs_uint32 t;
		int special;

		if ((p->di_mode & IFMT) != IFREG) {
		    /* This inode is not an AFS inode so 
		     * zero out all the spare fields.
		     */
		  zeroSpares:
		    if (!(p->di_flags & IC_PROPLIST)) {
			p->di_proplb = 0;
		    }
		    if (!(p->di_flags & IC_XUID)) {
			p->di_uid = 0;
		    }
		    if (!(p->di_flags & IC_XGID)) {
			p->di_gid = 0;
		    }
		    mod = 1;
		    ncnt++;
		    continue;
		}

/*	    printf("Inode %d: proplb=%u, uid=%u, bcuid=0x%x, bcgid=0x%x, gid=0x%x\n",
 *                 i , p->di_proplb, p->di_uid, p->di_bcuid, p->di_bcgid,
 *		       p->di_gid);
 */

		if (unconv) {	/*unconvert */
		    /* Check if the inode is not a DUX40D AFS inode */
		    if (p->di_proplb != VICEMAGIC
			|| (p->di_flags & IC_PROPLIST)) {
			ccnt++;
			if (verbose)
			    printf("   AFS Inode %d: already unconverted\n",
				   i);
			continue;
		    }

		    v1 = p->di_uid;
		    v2 = p->di_gid;
		    v3 = ((u_int) (p->di_bcuid)) << 16 |
			((u_int) (p->di_bcgid));

		    /* We have a sorted list of ufs inodes. Is it one of these */
		    while ((istep < icount) && (i > iarray[istep])) {
			istep++;
		    }
		    if ((istep < icount) && (i == iarray[istep])) {
			/* Yes, its a ufs inode */
			if (verbose)
			    printf("   Inode %d: Not an AFS Inode\n", i);
			goto zeroSpares;
		    }

		    /* Is it not an AFS inode. !IS_VICEMAGIC check */
		    if (!(v2 || v3)) {
			if (verbose)
			    printf("Warning: Inode %d: Unreferenced inode\n",
				   i);
			continue;	/* skip, making no change */
		    }

		    volid = v1;
		    if (((v2 >> 3) == 0x1fffffff) && (v2 & 0x3)) {
			special = 1;	/* INODESPECIAL */
			type = v2 & 0x3;
			rwvol = v3;
			if (verbose) {
			    printf
				("   %s AFS Inode %d: Vol=%u, RWVol=%u, Type=%d\n",
				 (force ? "Unconverting" :
				  "Would have unconverted"), i, volid, rwvol,
				 type);
			}
		    } else {
			special = 0;
			vnode = ((v2 >> 27) << 16) + (v3 & 0xffff);
			uniq = (v2 & 0x3fffff);
			vers =
			    (((v2 >> 22) & 0x1f) << 16) +
			    ((v3 >> 16) & 0xffff);
			if (verbose) {
			    printf
				("   %s AFS Inode %d: RWVol=%u, VNode=%u, Uniq=%u, Vers=%u\n",
				 (force ? "Unconverting" :
				  "Would have unconverted"), i, volid, vnode,
				 uniq, vers);
			}
		    }

		    /* Now unconvert the DUX40D AFS inode to the OSF30 version */
		    p->di_proplb = v1;
		    p->di_uid = v2;
		    p->di_gid = v3;
		    p->di_flags &= ~(IC_PROPLIST | IC_XUID | IC_XGID);
		    p->di_bcuid = 0;
		    p->di_bcgid = -2;
		    mod = 1;
		    cnt++;
		    wcnt++;

		} else {	/*convert */
		    if ((p->di_proplb == VICEMAGIC
			 && !(p->di_flags & IC_PROPLIST))
			|| (p->di_uid == 0 && p->di_gid == 0)
			|| (p->di_flags & (IC_XUID | IC_XGID))) {
			/* This inode is either already converted or not an AFS inode */
			/* We have a sorted list of ufs inodes. Is it one of these    */
			while ((istep < icount) && (i > iarray[istep])) {
			    istep++;
			}
			if ((istep < icount) && (i == iarray[istep])) {
			    /* Yes, its a ufs inode */
			    if (verbose) {
				printf("   Inode %d: Not an AFS Inode %s\n",
				       i, ((p->di_spare[1]
					    || p->
					    di_spare[2]) ? "(fixed)" : ""));
			    }
			    goto zeroSpares;
			}

			/* Is it a AFS inode. IS_VICEMAGIC check */
			if ((p->di_uid || p->di_gid)
			    && !(p->di_flags & (IC_XUID | IC_XGID))) {
			    ccnt++;
			    if (verbose)
				printf("   AFS Inode %d: already converted\n",
				       i);
			    continue;
			}

			/* Not an AFS nor UFS inode */
			printf("Warning: Inode %d: Unreferenced inode\n", i);
			continue;	/* skip, making no changes */
		    }

		    v1 = p->di_proplb;
		    v2 = p->di_uid;
		    v3 = p->di_gid;

		    volid = v1;
		    if (((v2 >> 3) == 0x1fffffff) && (v2 & 0x3)) {
			special = 1;	/* INODESPECIAL */
			type = v2 & 0x3;
			rwvol = v3;
			if (verbose) {
			    printf
				("   %s AFS Inode %d: Vol=%u, RWVol=%u, Type=%d\n",
				 (force ? "Converting" :
				  "Would have converted"), i, volid, rwvol,
				 type);
			}
		    } else {
			special = 0;
			vnode = ((v2 >> 27) << 16) + (v3 & 0xffff);
			uniq = (v2 & 0x3fffff);
			vers =
			    (((v2 >> 22) & 0x1f) << 16) +
			    ((v3 >> 16) & 0xffff);
			if (verbose) {
			    printf
				("   %s AFS Inode %d: RWVol=%u, VNode=%u, Uniq=%u, Vers=%u\n",
				 (force ? "Converting" :
				  "Would have converted"), i, volid,
				 (vnode & 0x1fffff), (uniq & 0x3fffff),
				 (vers & 0x1fffff));
			}
		    }

		    /* Now convert the OSF30 AFS inode to the DUX40D version */
		    p->di_proplb = VICEMAGIC;
		    p->di_uid = v1;
		    p->di_gid = v2;
		    p->di_bcuid = (u_short) (v3 >> 16);
		    p->di_bcgid = (u_short) v3;
		    p->di_flags &= ~(IC_PROPLIST);
		    p->di_flags |= (IC_XUID | IC_XGID);
		    mod = 1;
		    cnt++;
		    wcnt++;
		}		/*convert */
	    }
#else
	    {
		afs_uint32 p1, p2, p3, p4;
		int p5;
		quad *q;

#if	defined(AFS_SUN56_ENV)
		q = (quad *) & (p->di_ic.ic_lsize);
#else
		q = &(p->di_ic.ic_size);
#endif

		p1 = p->di_gen;
		p2 = p->di_ic.ic_flags;
		p3 = q->val[0];
		p4 = p->di_ic.ic_uid;
		p5 = p->di_ic.ic_gid;

		/* the game is afoot, Dr Watson! */
		if (!verbose && !((ccnt + cnt + ncnt) % 5000)) {
		    printf(".");
		    fflush(stdout);
		}

		if (unconv) {
		    /*
		     * Convert from a 5.6 format to a pre 5.6 format
		     */
		    if ((p2 || p3) && !p4 && (p5 == -2)) {
			if (verbose)
			    printf
				("AFS Inode %d: Already in pre-%s AFS format (%x,%x,%x,%x,%x) ignoring..\n",
				 i, OSVERS, p1, p2, p3, p4, p5);
			ccnt++;
			continue;
		    }

		    if (p5 == VICEMAGIC) {
			/* This is a sol 2.6 AFS inode */
			mod = 1;
			cnt++;
			wcnt++;
			if (verbose)
			    printf
				("AFS Inode %d: %s from Sol5.6 (%x,%x,%x,%x,%x) \n",
				 i,
				 (force ? "Unconverting" :
				  "Would have unconverted"), p1, p2, p3, p4,
				 p5);
			if (force) {
			    q->val[0] = p->di_uid;
			    p->di_uid = 0;
			    p->di_gid = -2;
			    p->di_suid = UID_LONG;
			    p->di_sgid = GID_LONG;
			}
			continue;
		    }
		} else {
		    if (p5 == VICEMAGIC) {	/* Assume an already converted 5.6 afs inode */
			if (verbose)
			    printf
				("AFS Inode %d: Already in %s AFS format (p1=%x,p2=%x,p3=%x,p4=%x,p5=%x); ignoring..\n",
				 i, OSVERS, p1, p2, p3, p4, p5);
			ccnt++;
			continue;
		    }

		    /* for inodes created in solaris 2.4, there is a possibility
		     ** that the gid gets chopped off to an unsigned short(2 bytes)
		     */
		    if ((p2 || p3) && !p4 && ((p5 == -2)
					      || ((unsigned short)p5 ==
						  (unsigned short)-2))) {
			/* This is a pre Sol2.6 inode */
			mod = 1;
			cnt++;
			wcnt++;
			if (verbose)
			    printf
				("AFS Inode %d: %s to 5.6 format (p1=%x,p2=%x,p3=%x,p4=%x, p5=%x)\n",
				 i,
				 (force ? "Converting" :
				  "Would have converted"), p1, p2, p3, p4,
				 p5);
			if (force) {
			    p->di_gid = VICEMAGIC;
			    p->di_uid = q->val[0];
			    p->di_suid = UID_LONG;
			    p->di_sgid = GID_LONG;
			    q->val[0] = 0;
			}
			continue;
		    }
		}
		/* If not an AFS inode, ignore */
		ncnt++;
		if (verbose)
		    printf
			("Non AFS Inode %d: (p1=%x,p2=%x,p3=%x, p4=%x,p5=%x) ignoring..\n",
			 i, p1, p2, p3, p4, p5);
	    }
#endif

	    if (mod && force) {
		if (lseek(pfd, -bufsize, SEEK_CUR) == -1) {	/* Point to loc bef read */
		    printf("Error Seeking backwards %d bytes\n", bufsize);
		    printf("errno %d: %s\n", errno, strerror(errno));
		    goto out;
		}
		code = write(pfd, inodes, bufsize);
		if (code != bufsize) {	/* Update inodes */
		    printf("Error writing modified inodes for partition %s\n",
			   partition);
		    if (code < 0)
			printf("errno %d: %s\n", errno, strerror(errno));
		    goto out;
		}
		if (verbose) {
		    printf
			("  Write %d AFS inodes and %d non-AFS inodes to disk\n",
			 wcnt, ncnt);
		}
	    }
	}
    }

    printf("\n%s: %d AFS inodes %s ", partition, cnt,
	   (force ? "were" : "would have been"));
    if (unconv)
	printf("unconverted to a pre-%s format; %d already unconverted.\n",
	       OSVERS, ccnt);
    else
	printf("converted to a %s format; %d already converted.\n", OSVERS,
	       ccnt);

    close(pfd);
    return 0;

  out:
    close(pfd);
    return -1;
}




int
bread(fd, buf, blk, size)
     int fd;
     char *buf;
     daddr_t blk;
     afs_int32 size;
{
    if (lseek(fd, (off_t) dbtob(blk), L_SET) < 0
	|| read(fd, buf, size) != size) {
	printf("bread: lseek failed (errno = %d)\n", errno);
	return -1;
    }
    return 0;
}


/* 
 * Ensure that we don't have a "/" instead of a "/dev/rxd0a" type of device.
 * returns pointer to static storage; copy it out quickly!
 */
char *
vol_DevName(adev)
     dev_t adev;
{
    struct dirent *dp;
    static char pbuffer[128];
    struct stat tstat;
    DIR *dirp;
    char *dirName;
    int code;

    /* now, look in /dev for the appropriate file */
    dirp = opendir(dirName = "/dev");
    while (dp = readdir(dirp)) {
	strcpy(pbuffer, dirName);
	strcat(pbuffer, "/");
	strcat(pbuffer, dp->d_name);
	if (stat(pbuffer, &tstat) != -1 && (tstat.st_mode & S_IFMT) == S_IFBLK
	    && (tstat.st_rdev == adev)) {
	    strcpy(pbuffer, dp->d_name);
	    closedir(dirp);
	    return pbuffer;
	}
    }
    closedir(dirp);
    return NULL;		/* failed */
}

char *
unrawname(name)
     char *name;
{
    char *dp;
    struct stat stb;

    if ((dp = strrchr(name, '/')) == 0)
	return (name);
    if (stat(name, &stb) < 0)
	return (name);
    if ((stb.st_mode & S_IFMT) != S_IFCHR)
	return (name);
    if (*(dp + 1) != 'r')
	return (name);
    (void)strcpy(dp + 1, dp + 2);
    return (name);
}

char *
rawname(name)
     char *name;
{
    static char rawbuf[32];
    char *dp;

    if ((dp = strrchr(name, '/')) == 0)
	return (0);
    *dp = 0;
    (void)strcpy(rawbuf, name);
    *dp = '/';
    (void)strcat(rawbuf, "/r");
    (void)strcat(rawbuf, dp + 1);
    return (rawbuf);
}

char *
blockcheck(name)
     char *name;
{
    struct stat stslash, stblock, stchar;
    char *raw;
    int retried = 0;

  retry:
    if (stat(name, &stblock) < 0) {
	perror(name);
	printf("Can't stat %s\n", name);
	return (0);
    }
    if ((stblock.st_mode & S_IFMT) == S_IFBLK) {
	raw = rawname(name);
	if (stat(raw, &stchar) < 0) {
	    perror(raw);
	    printf("Can't stat %s\n", raw);
	    return (name);
	}
	if ((stchar.st_mode & S_IFMT) == S_IFCHR) {
	    return (raw);
	} else {
	    printf("%s is not a character device\n", raw);
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


/* ensure that we don't have a "/" instead of a "/dev/rxd0a" type of device.
 * Overwrites abuffer with the corrected name.
 */
EnsureDevice(abuffer)
     char *abuffer;
{
    struct dirent *dp;
    char pbuffer[128];
    struct stat tstat;
    DIR *dirp;
    char *dirName;
    int code;
    short dev;

    code = stat(abuffer, &tstat);
    if (code)
	return code;
    if (((tstat.st_mode & S_IFMT) == S_IFBLK)
	|| ((tstat.st_mode & S_IFMT) == S_IFCHR)) {
	return 0;		/* already a block or char device */
    }
    /* otherwise, assume we've got a normal file, and we look up its device */
    dev = tstat.st_dev;		/* remember device for this file */

    /* now, look in /dev for the appropriate file */
    dirp = opendir(dirName = "/dev");

    while (dp = readdir(dirp)) {
	strcpy(pbuffer, dirName);
	strcat(pbuffer, "/");
	strcat(pbuffer, dp->d_name);
	if (stat(pbuffer, &tstat) != -1 && (tstat.st_mode & S_IFMT) == S_IFBLK
	    && (tstat.st_rdev == dev)) {
	    strcpy(abuffer, pbuffer);
	    closedir(dirp);
	    return 0;
	}
    }
    closedir(dirp);
    return 1;			/* failed */
}
#else

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char *argv[];
{
    printf
	("%s: **ONLY** supported for SunOS 3.5-4.x and dux4.0D systems ...\n",
	 argv[0]);
}
#endif /*       defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV) */

#if	defined(AFS_SUN54_ENV)

/*
** Open the /etc/vfstab file to map the raw device name to the mountable
** device name. Then open the /etc/mnttab file to see if this mountable 
** device is mounted.
**
** Returns 1 if the conversion process should continue, otherwise returns 0
*/
CheckMountedDevice(devName)
     char *devName;		/* raw device name */
{
    FILE *vfsent, *mntent;
    struct mnttab mnt;
    struct vfstab vnt;
    char *unRawDev = 0;
    char YesNo = 'y';
    int found = 0;

    if ((vfsent = fopen(VFSTAB, "r")) == NULL) {
	printf("Unable to open %s ( errno = %d)\n", VFSTAB, errno);
	exit(2);
    }
    while (!getvfsent(vfsent, &vnt)) {
	char *rawDev = vnt.vfs_fsckdev;
	if (rawDev && !strcmp(rawDev, devName)) {
	    /* this is the device we are looking for */
	    unRawDev = vnt.vfs_special;
	    break;
	}
    }
    fclose(vfsent);

    if (!unRawDev)
	goto done;		/* could not find it in /etc/vfstab */

    /* we found the entry in /etc/vfstab. Now we open /etc/mnnttab and
     ** verify that it is not mounted 
     */
    if ((mntent = fopen(MNTTAB, "r")) == NULL) {
	printf("Unable to open %s ( errno = %d)\n", VFSTAB, errno);
	exit(2);
    }

    while (!getmntent(mntent, &mnt)) {
	char *resource = mnt.mnt_special;
	if (resource && !strcmp(resource, unRawDev)) {
	    found = 1;
	    break;
	}
    }
    fclose(mntent);


    /* if we found an entry in the /etc/mnttab file, then this 
     ** device must be mounted
     */
    if (found) {
      done:
	do {
	    printf
		("Device %s may be mounted. Can corrupt data. Continue anyway(y/n)?",
		 devName);
	    fflush(stdout);
	    fflush(stdin);
	    YesNo = getc(stdin);
	}
	while (YesNo != 'y' && YesNo != 'Y' && YesNo != 'n' && YesNo != 'N');
    }
    if ((YesNo == 'y') || (YesNo == 'Y'))
	return 1;
    return 0;
}

#endif /* AFS_SUN54_ENV */
