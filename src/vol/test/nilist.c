/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* nilist.c - List the "inode" information for one or all volumes on
 * a partition.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <windows.h>
#include <winbase.h>
#endif
#include "nfs.h"
#include <afs/afsint.h>
#include "ihandle.h"
#include <lock.h>
#include "vnode.h"
#include "volume.h"
#include "viceinode.h"

#ifndef AFS_NAMEI_ENV
main()
{
    printf("nilist is only useful for namei AFS file server"
	   " implementations.\n");
    exit(1);
}
#else

void
Usage(void)
{
    printf("Usage: nilist partition [volume]\n");
    printf
	("List all \"inodes\" for the volume group containing the volume\n");
    printf("or for the entire partition.\n");
    exit(1);
}

/* This judge function can be a dummy since I know how nt_ListAFSFiles works */
int
Judge(struct ViceInodeInfo *info, int vid)
{
    return 1;
}

int
PrintInodeInfo(FILE * fp, struct ViceInodeInfo *info, char *dir, char *name)
{
    static int lastVID = -1;
    int rwVID;
    char dname[1024];

    rwVID =
	info->u.param[1] ==
	-1 ? info->u.special.parentId : info->u.vnode.volumeId;

    if (rwVID != lastVID) {
	if (lastVID != -1)
	    printf("\n");
	lastVID = rwVID;
	/* This munging of the name remove a "\R". */
	(void)strcpy(dname, dir);
	dname[strlen(dname) - 2] = '\0';
	printf("Parent Volume %d, Directory %s\n", rwVID, dname);
	printf("%19s %8s %5s %10s %10s %10s %10s %s\n", "Inode", "Size",
	       "Nlink", "P1", "P2", "P3", "P4", "Name");
    }
#ifdef AFS_NT40_ENV
    printf("%19I64d %8d %5d %10d %10d %10d %10d %s\n", info->inodeNumber,
	   info->byteCount, info->linkCount, info->u.param[0],
	   info->u.param[1], info->u.param[2], info->u.param[3], name);
#else
    printf("%19lld %8d %5d %10d %10d %10d %10d %s\n", info->inodeNumber,
	   info->byteCount, info->linkCount, info->u.param[0],
	   info->u.param[1], info->u.param[2], info->u.param[3], name);
#endif

    return 0;
}

main(int ac, char **av)
{
    int singleVolumeNumber = 0;
    char *part;
    int ninodes;

    if (ac < 2 || ac > 3)
	Usage();

    part = av[1];
    if (ac == 3)
	singleVolumeNumber = atoi(av[2]);

#ifdef AFS_NT40_ENV
    ninodes =
	nt_ListAFSFiles(part, PrintInodeInfo, stdout, Judge,
			singleVolumeNumber);
#else
    ninodes =
	namei_ListAFSFiles(part, PrintInodeInfo, stdout, Judge,
			   singleVolumeNumber);
#endif
}


#endif /* AFS_NAMEI_ENV */
