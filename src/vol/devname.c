/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/param.h>
#include <afsconfig.h>

RCSID
    ("$Header$");

#include <afs/param.h>
#include <rx/xdr.h>
#include <afs/afsint.h>
#include <ctype.h>
#if !defined(AFS_SGI_ENV)
#ifdef	AFS_OSF_ENV
#include <ufs/fs.h>
#else /* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
#define VFS
#ifdef	AFS_SUN5_ENV
#include <sys/fs/ufs_fs.h>
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#else
#include <ufs/fs.h>
#endif
#endif
#else /* AFS_VFSINCL_ENV */
#if !defined(AFS_AIX_ENV) && !defined(AFS_LINUX22_ENV) && !defined(AFS_XBSD_ENV)
#include <sys/fs.h>
#endif
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_OSF_ENV */
#endif /* AFS_SGI_ENV */
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#ifdef	AFS_AIX_ENV
#include <sys/vfs.h>
#include <fcntl.h>
#else
#ifdef	AFS_HPUX_ENV
#include <fcntl.h>
#include <mntent.h>
#else
#if	defined(AFS_SUN_ENV) || defined(AFS_SUN5_ENV)
#ifdef	AFS_SUN5_ENV
#include <sys/mnttab.h>
#include <sys/mntent.h>
#else
#include <mntent.h>
#endif
#else
#if defined(AFS_SGI_ENV)
#include <fcntl.h>
#include <mntent.h>
#else
#ifdef AFS_LINUX22_ENV
#include <mntent.h>
#else
#include <fstab.h>
#endif
#endif
#endif /* AFS_SGI_ENV */
#endif /* AFS_HPUX_ENV */
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <setjmp.h>
#ifndef ITIMER_REAL
#include <sys/time.h>
#endif /* ITIMER_REAL */
#include "partition.h"


#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


/* ensure that we don't have a "/" instead of a "/dev/rxd0a" type of device.
 * returns pointer to static storage; copy it out quickly!
 */
char *
vol_DevName(dev_t adev, char *wpath)
{
    static char pbuffer[128];
    char pbuf[128], *ptr;
    int code, i;
#ifdef	AFS_SUN5_ENV
    struct mnttab mnt;
    FILE *mntfile;
#else
#if defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_LINUX22_ENV)
    struct mntent *mntent;
    FILE *mfd;
#else
    struct fstab *fsent;
#endif
#endif
#ifdef	AFS_AIX_ENV
    int nmounts;
    struct vmount *vmountp;
#endif

#ifdef	AFS_AIX_ENV
    if ((nmounts = getmount(&vmountp)) <= 0) {
	return NULL;
    }
    for (; nmounts;
	 nmounts--, vmountp =
	 (struct vmount *)((int)vmountp + vmountp->vmt_length)) {
	char *part = vmt2dataptr(vmountp, VMT_STUB);
#else
#ifdef	AFS_SUN5_ENV
    if (!(mntfile = fopen(MNTTAB, "r"))) {
	return NULL;
    }
    while (!getmntent(mntfile, &mnt)) {
	char *part = mnt.mnt_mountp;
#else
#if defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_LINUX22_ENV)
#ifdef AFS_LINUX22_ENV
    if ((mfd = setmntent("/proc/mounts", "r")) == NULL) {
	if ((mfd = setmntent("/etc/mtab", "r")) == NULL) {
	    return NULL;
	}
    }
#else
    if ((mfd = setmntent(MOUNTED /*MNTTAB*/, "r")) == NULL) {
	return NULL;
    }
#endif
    while ((mntent = getmntent(mfd))) {
	char *part = mntent->mnt_dir;
#else
    setfsent();
    while ((fsent = getfsent())) {
	char *part = fsent->fs_file;
#endif
#endif /* AFS_SGI_ENV */
#endif
	struct stat status;
#ifdef	AFS_AIX_ENV
	if (vmountp->vmt_flags & (MNT_READONLY | MNT_REMOVABLE | MNT_REMOTE))
	    continue;		/* Ignore any "special" partitions */
#else
#ifdef	AFS_SUN5_ENV
	/* Ignore non ufs or non read/write partitions */
	if ((strcmp(mnt.mnt_fstype, "ufs") != 0)
	    || (strncmp(mnt.mnt_mntopts, "ro,ignore", 9) == 0))
	    continue;
#else
#if defined(AFS_LINUX22_ENV)
	if (strcmp(mntent->mnt_type, "ext2"))
	    continue;
#else
#if defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_HPUX_ENV)
	if (!hasmntopt(mntent, MNTOPT_RW))
	    continue;
#else
	if (strcmp(fsent->fs_type, "rw") != 0)
	    continue;		/* Ignore non read/write partitions */
#endif /* AFS_LINUX22_ENV */
#endif /* AFS_SGI_ENV */
#endif
#endif
	/* Only keep track of "/vicepx" partitions since it can get hairy when NFS mounts are involved.. */
	if (strncmp(part, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE)) {
	    continue;		/* Non /vicepx; ignore */
	}
	if (stat(part, &status) == -1) {
	    continue;
	}
#if !defined(AFS_SGI_XFS_IOPS_ENV) && !defined(AFS_LINUX22_ENV)
	if ((status.st_ino !=
	     ROOTINO) /*|| ((status.st_mode & S_IFMT) != S_IFBLK) */ ) {
	    continue;
	}
#endif
	if (status.st_dev == adev) {
#ifdef	AFS_AIX_ENV
	    strcpy(pbuffer, vmt2dataptr(vmountp, VMT_OBJECT));
#else
#ifdef	AFS_SUN5_ENV
	    strcpy(pbuffer, mnt.mnt_special);
#else
#if defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_LINUX22_ENV)
	    strcpy(pbuffer, mntent->mnt_fsname);
#else
	    strcpy(pbuffer, fsent->fs_spec);
#endif
#endif /* AFS_SGI_ENV */
#endif
	    if (wpath) {
		strcpy(pbuf, pbuffer);
		ptr = (char *)strrchr(pbuf, '/');
		if (ptr) {
		    *ptr = '\0';
		    strcpy(wpath, pbuf);
		} else
		    return NULL;
	    }
	    ptr = (char *)strrchr(pbuffer, '/');
	    if (ptr) {
		strcpy(pbuffer, ptr + 1);
		return pbuffer;
	    } else
		return NULL;
	}
    }
#ifdef	AFS_SUN5_ENV
    (void)fclose(mntfile);
#else
#if defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_LINUX22_ENV)
    endmntent(mfd);
#else
#ifndef	AFS_AIX_ENV
    endfsent();
#endif
#endif
#endif /* AFS_SGI_ENV */
    return NULL;
}

/* Search for the raw device name. Will put an "r" in front of each
 * directory and file entry of the pathname until we find a character
 * device.
 */
char *
afs_rawname(char *devfile)
{
    static char rawname[100];
    struct stat statbuf;
    char *p;
    int code, i;

    i = strlen(devfile);
    while (i >= 0) {
	strcpy(rawname, devfile);
	if (devfile[i] == '/') {
	    rawname[i + 1] = 'r';
	    rawname[i + 2] = 0;
	    strcat(rawname, &devfile[i + 1]);
	}

	code = stat(rawname, &statbuf);
	if (!code && S_ISCHR(statbuf.st_mode))
	    return rawname;

	while ((--i >= 0) && (devfile[i] != '/'));
    }

    return NULL;
}
