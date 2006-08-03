/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Verify that the size of the XFS inode is large enough to hold the XFS
 * attribute for AFS inode parameters. Check all the mounted /vicep partitions.
#include <afsconfig.h>

RCSID("$Header$");

 */
#include <afs/param.h>
#ifdef AFS_SGI_XFS_IOPS_ENV
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <mntent.h>
#include "partition.h"
#include <afs/xfsattrs.h>

char *prog = "xfs_size_check";

/* Verify that the on disk XFS inodes on the partition are large enough to
 * hold the AFS attribute. Returns -1 if the attribute can't be set or is
 * too small to fit in the inode. Returns 0 if the attribute does fit in
 * the XFS inode.
 */
#define VERIFY_ERROR   -1
#define VERIFY_OK	0
#define VERIFY_FIX	1
static int
VerifyXFSInodeSize(char *part)
{
    afs_xfs_attr_t junk;
    int length = SIZEOF_XFS_ATTR_T;
    int fd;
    int code = VERIFY_ERROR;
    struct fsxattr fsx;

    if (attr_set(part, AFS_XFS_ATTR, &junk, length, ATTR_ROOT) < 0) {
	if (errno == EPERM) {
	    printf("Must be root to run %s\n", prog);
	    exit(1);
	}
	return VERIFY_ERROR;
    }

    if ((fd = open(part, O_RDONLY, 0)) < 0)
	goto done;

    if (fcntl(fd, F_FSGETXATTRA, &fsx) < 0)
	goto done;

    if (fsx.fsx_nextents == 0)
	code = VERIFY_OK;
    else
	code = VERIFY_FIX;

  done:
    if (fd >= 0)
	close(fd);
    (void)attr_remove(part, AFS_XFS_ATTR, ATTR_ROOT);

    return code;
}

#define ALLOC_STEP 256
#define NAME_SIZE 64
typedef struct {
    char partName[NAME_SIZE];
    char devName[NAME_SIZE];
} partInfo;
partInfo *partList = NULL;
int nParts = 0;
int nAvail = 0;

int
CheckPartitions()
{
    int i;
    struct mntent *mntent;
    FILE *mfd;
    DIR *dirp;
    struct dirent *dp;
    struct stat status;
    int code;

    if ((mfd = setmntent(MOUNTED, "r")) == NULL) {
	printf("Problems in getting mount entries(setmntent): %s\n",
	       strerror(errno));
	exit(-1);
    }
    while (mntent = getmntent(mfd)) {
	char *part = mntent->mnt_dir;

	if (!hasmntopt(mntent, MNTOPT_RW))
	    continue;

	if (strncmp(part, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE)) {
	    continue;		/* Non /vicepx; ignore */
	}
	if (stat(part, &status) == -1) {
	    printf("Couldn't find file system %s; ignored\n", part);
	    continue;
	}
	if (!strcmp("xfs", status.st_fstype)) {
	    code = VerifyXFSInodeSize(part);
	    switch (code) {
	    case VERIFY_OK:
		break;
	    case VERIFY_ERROR:
		printf("%s: Can't check XFS inode size: %s\n",
		       strerror(errno));
		break;
	    case VERIFY_FIX:
		if (nAvail <= nParts) {
		    nAvail += ALLOC_STEP;
		    if (nAvail == ALLOC_STEP)
			partList =
			    (partInfo *) malloc(nAvail * sizeof(partInfo));
		    else
			partList =
			    (partInfo *) realloc((char *)partList,
						 nAvail * sizeof(partInfo));
		    if (!partList) {
			printf
			    ("Failed to %salloc %d bytes for partition list.\n",
			     (nAvail == ALLOC_STEP) ? "m" : "re",
			     nAvail * sizeof(partInfo));
			exit(1);
		    }
		}
		(void)strcpy(partList[nParts].partName, part);
		(void)strcpy(partList[nParts].devName, mntent->mnt_fsname);
		nParts++;
		break;
	    default:
		printf("Unknown return code %d from VerifyXFSInodeSize.\n",
		       code);
		abort();
	    }
	}
    }
    return nParts;
}


main(int ac, char **av)
{
    int i;
    int code;

    if (getuid()) {
	printf("Must be root to run %s.\n", prog);
	exit(1);
    }

    code = CheckPartitions();
    if (code) {
	printf("Need to remake the following partitions:\n");
	for (i = 0; i < nParts; i++) {
	    printf("%s: mkfs -t xfs -i size=512 -l size=4000b %s\n",
		   partList[i].partName, partList[i].devName);
	}
    }
    exit(code ? 1 : 0);
}


#else /* AFS_SGI_XFS_IOPS_ENV */
main()
{
    printf("%s only runs on XFS platforms.\n, prog");
    exit(1);
}
#endif /* AFS_SGI_XFS_IOPS_ENV */
