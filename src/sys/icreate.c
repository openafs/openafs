
#ifndef lint
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <afs/param.h>

#include "AFS_component_version_number.c"

main(argc,argv)
char **argv;
{
    int inode;
    struct stat status;
#ifdef AFS_SGI61_ENV
    int vnode, unique, datav;
#else /* AFS_SGI61_ENV */
    afs_int32 vnode, unique, datav;
#endif /* AFS_SGI61_ENV */

    if (stat("/vicepa", &status) == -1) {
	perror("stat");
	exit(1);
    }
    vnode = atoi(argv[1]);
    unique = atoi(argv[2]);
    datav = atoi(argv[3]);
    inode = icreate(status.st_dev, 0, 17, vnode, unique, datav);
    if (inode == -1) {
	perror("icreate");
	exit(1);
    }
    printf("icreate successful, inode=%d\n", inode);
    exit(0);
}
