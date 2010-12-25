/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* nicreate
 * Test "inode" creation in the user space file system.
 */

#include <afsconfig.h>
#include <afs/param.h>


#ifdef AFS_NAMEI_ENV
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <lock.h>
#include <afs/afsutil.h>
#include "nfs.h"
#include <afs/afsint.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "viceinode.h"
#include "voldefs.h"
#include "partition.h"
#include <dirent.h>
#include <afs/afs_assert.h>


char *prog = "nicreate";
IHandle_t *GetLinkHandle(char *part, int volid);

void
Usage(void)
{
    printf("Usage: %s partition RWvolid p1 p2 p3 p4\n", prog);
    exit(1);
}

main(int ac, char **av)
{
    char *part;
    int volid;
    int p1, p2, p3, p4;
    IHandle_t lh, *lhp;
    Inode ino;

    if (ac != 7)
	Usage();

    part = av[1];
    volid = atoi(av[2]);
    p1 = atoi(av[3]);
    p2 = atoi(av[4]);
    p3 = atoi(av[5]);
    p4 = atoi(av[6]);

    if (p2 == -1 && p3 == VI_LINKTABLE)
	lhp = NULL;
    else {
	lhp = GetLinkHandle(part, volid);
	if (!lhp) {
	    perror("Getting link handle.\n");
	    exit(1);
	}
    }

    ino = namei_icreate(lhp, part, p1, p2, p3, p4);
    if (!VALID_INO(ino)) {
	perror("namei_icreate");
    } else {
	printf("Returned inode %s\n", PrintInode(NULL, ino));
    }
}

#endif /* AFS_NAMEI_ENV */
