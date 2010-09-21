/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
This utility is used to increase the ref count on inodes moved to
lost+found by the non-AFS fsck. The program needs to be run once
for every volume located on the partition that was fsck'ed.
Procedure:
     cc -o fixit fixit.c
     cd /vicepx
     chmod 600 *
     foreach volid ( cat <list-of-vol-ids> )
       fixit lost+found $volid
       echo $volid
     end
     umount <dev>
     /etc/vfsck <dev>       <<<< AFS Version!

Non-AFS fsck causes inodes to be moved to lost+found, with names like
#<inode-no>. The volumes associated with these inodes will still be
available, until the #<inode-no> file is removed. This program simply
ups the ref count on the #<inode-no> files, so they're not returned
to the free list when the files are removed from lost+found
*/

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <sys/file.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef AFS_HPUX_ENV
#include <sys/mknod.h>
#endif
#include <afs/afs_args.h>
#include <afs/afs.h>
#include <afs/afssyscalls.h>
#include <errno.h>

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    DIR *tdir;
    struct stat ts;
    afs_int32 dev, code;
    struct dirent *tde;

    int volid;

    if (geteuid() != 0) {
	printf("must be run as root; sorry\n");
	exit(1);
    }
    code = stat(argv[1], &ts);
    if (code) {
	printf("can't stat %s\n", argv[1]);
	exit(1);
    }
    dev = ts.st_dev;
    tdir = opendir(argv[1]);
    if (!tdir) {
	printf("cant open %s\n", argv[1]);
	exit(1);
    }
    volid = atoi(argv[2]);
    for (tde = readdir(tdir); tde; tde = readdir(tdir)) {
	if (tde->d_name[0] == '#') {
	    printf("Inode %d\n", tde->d_ino);
	    code = IINC(dev, tde->d_ino, volid);
	    if (code == -1) {
		perror("iinc");
		printf("errno = %d\n", errno);
/* Remove this -- we don't want to exit, because we have to look
*  at each inode -- an error here means only that the iinc failed for
*  the current volume
*				exit(1);
*/
	    } else
		printf("inode %d restored for volume %d\n", tde->d_ino,
		       volid);
	}
    }
    exit(0);
}
