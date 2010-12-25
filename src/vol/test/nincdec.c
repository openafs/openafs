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


char *prog = "nincdec";
IHandle_t *GetLinkHandle(char *part, int volid);


void
Usage(void)
{
    printf("Usage: %s <part> <volid> <-i ino | -v vno uniq tag> <inc|dec>\n",
	   prog);
    exit(1);
}

main(int ac, char **av)
{
    char *part;
    int volid;
    Inode ino;
    Inode vno;
    Inode tag;
    Inode uniq;
    int code;
    IHandle_t *lh;
    int do_inc = -1;
    char *incdecarg;
    int i;


    if (ac < 5)
	Usage();

    part = av[1];
    volid = atoi(av[2]);

    i = 4;
    if (!strcmp(av[3], "-i")) {
	code = sscanf(av[i++], "%Lu", &ino);
	if (code != 1) {
	    printf("Failed to get inode from %s\n", av[4]);
	    exit(1);
	}
    } else if (!strcmp(av[3], "-v")) {
	vno = (int64_t) atoi(av[i++]);
	vno &= 0x3ffffff;
	tag = (int64_t) atoi(av[i++]);
	uniq = (int64_t) atoi(av[i++]);
	ino = uniq;
	ino |= tag << 32;
	ino |= vno << 35;
	printf("ino=%Lu\n", ino);
    } else {
	printf("Expected \"-i\" or \"-v\" for inode value\n");
	Usage();
    }

    incdecarg = av[i++];
    if (!strcmp(incdecarg, "dec"))
	do_inc = 0;
    else if (!strcmp(incdecarg, "inc"))
	do_inc = 1;
    else {
	printf("%s: Expected \"inc\" or \"dec\"\n", incdecarg);
	Usage();
    }


    lh = GetLinkHandle(part, volid);
    if (!lh) {
	printf("Failed to get link handle, exiting\n");
	exit(1);
    }

    if (do_inc)
	code = namei_inc(lh, ino, volid);
    else
	code = namei_dec(lh, ino, volid);

    printf("namei_%s returned %d\n", do_inc ? "inc" : "dec", code);

    exit(0);

}

#endif /* AFS_NAMEI_ENV */
