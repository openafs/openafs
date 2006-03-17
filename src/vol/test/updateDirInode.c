/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
**	Lists all files in a /vicepX partition. This started as an
**	exercise to know the layout of AFS data in a fileserver
**	partition. Later on, it proved useful in debugging problems 
**	at customer sites too. 
**
*/
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <rx/xdr.h>
#include <afs/afsint.h>
#include <ctype.h>
#include <sys/param.h>
#if !defined(AFS_SGI_ENV)
#ifdef	AFS_OSF_ENV
#include <ufs/fs.h>
#else /* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
#define VFS
#ifdef	AFS_SUN5_ENV
#include <sys/fs/ufs_fs.h>
#else
#include <ufs/fs.h>
#endif
#else /* AFS_VFSINCL_ENV */
#ifndef	AFS_AIX_ENV
#include <sys/fs.h>
#endif
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_OSF_ENV */
#endif /* AFS_SGI_ENV */
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#if defined(AFS_SGI_ENV) || (defined(AFS_SUN_ENV) && !defined(AFS_BSD_ENV))
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
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

/*
#include <sys/fs/efs.h>
*/
#include "efs.h"		/* until 5.1 release */

#define ROOTINO EFS_ROOTINO
#else
#include <fstab.h>
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

#include "nfs.h"
#include <afs/errors.h>
#include "lock.h"
#include "lwp.h"
#include "vnode.h"
#include "volume.h"
#include "vldb.h"
#include "partition.h"
#include "afs/assert.h"
#include "filesignal.h"
#include "vutils.h"
#include "daemon_com.h"
#include "fssync.h"
#include <afs/auxinode.h>
#include <afs/dir.h>
#include <unistd.h>

#ifdef	AFS_OSF_ENV
extern void *calloc(), *realloc();
#endif
#include "salvage.h"
int volumeId;
int VolumeChanged;		/* to satisfy library libdir use */

#include "listVicepx.h"
char *orphan_NoVnode = "ORPHANED_NoVnode";
char *orphan_NoUnique = "ORPHANED_NoUnique";
#define READBUFSIZE	5*1024


#define	allNull		0x00
#define verbose		0x01
#define	update		0x02	/* update specified dir inode */

int
Usage(name)
     char *name;
{
    assert(name);
    printf
	("Usage is %s -p <partition name> -v <volume name> -dirInode <directory inode> -f <directoryFileName> -verbose\n",
	 name);
    exit(1);
}


main(argc, argv)
     int argc;
     char *argv[];
{
    char fullName[32 + VNAMESIZE + sizeof(VHDREXT) + 4];
    char partition[32], volume[VNAMESIZE];
    struct stat statBuf;
    struct VolumeHeader volumeHeader;
    int fd, i, sawPart = 0, sawVolume = 0, sawDirContents = 0, sawDirFile = 0;
    char option = allNull;	/* no default options */
    Inode dirInode;		/* destination dir Inode */
    char *fileName;		/* source directory file */

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-p")) {
	    if ((i + 1) >= argc)
		Usage(argv[0]);
	    assert(strlen(argv[i + 1]) < 32);
	    strcpy(partition, argv[++i]);
	    sawPart = 1;
	} else if (!strcmp(argv[i], "-v")) {
	    if ((i + 1) >= argc)
		Usage(argv[0]);
	    assert(strlen(argv[i + 1]) < VNAMESIZE);
	    strcpy(volume, argv[++i]);
	    sawVolume = 1;
	} else if (!strcmp(argv[i], "-dirInode")) {
	    if ((i + 1) >= argc)
		Usage(argv[0]);
	    dirInode = atoi(argv[++i]);
	    sawDirContents = 1;
	    option |= update;
	} else if (!strcmp(argv[i], "-f")) {
	    if ((i + 1) >= argc)
		Usage(argv[0]);
	    fileName = argv[++i];
	    sawDirFile = 1;
	} else if (!strcmp(argv[i], "-verbose"))
	    option |= verbose;
	else
	    Usage(argv[0]);
    }

    /* option to verify whether input file is syntactically good */
    if (sawDirFile && !sawPart && !sawVolume && !sawDirContents) {
	scanDirFile(0, 0, fileName, option);
	exit(2);
    }

    /* check input parameters */
    if (!sawPart || !sawVolume || !sawDirFile || !sawDirContents)
	Usage(argv[0]);

    /* extract volume id */
    volumeId = atoi(volume);

    /* construct  unix file name */
    strcpy(fullName, partition);
    strcat(fullName, "/V");
    strcat(fullName, volume);
    strcat(fullName, VHDREXT);

    /* check to see that volume exists */
    if (stat(fullName, &statBuf) < 0) {
	printf("Error in stat(%s) : %d\n", fullName, errno);
	exit(2);
    }

    /* read volume header */
    if ((fd = open(fullName, O_RDONLY)) < 0) {
	printf("Error in open(%s) : %d\n", fullName, errno);
	exit(3);
    }
    if (read(fd, &volumeHeader, sizeof(struct VolumeHeader)) <
	sizeof(struct VolumeHeader)) {
	printf("Error in reading Volume Header : %d\n", errno);
	exit(4);
    }

    scanDirFile(statBuf.st_dev, dirInode, fileName, option);
    close(fd);
}

int
scanDirFile(dev, node, fileName, option)
     dev_t dev;
     Inode node;		/* destination directory inode number */
     char *fileName;		/* source file to update from */
     char option;		/* user options */
{
    int fd, i, j, temp;
    FILE *fp;
    char dirPage[2048];
    char buf[READBUFSIZE];	/* read buffer */
    struct DirHeader *dhp = (struct DirHeader *)&dirPage[0];
    struct stat statBuf;
    int pgCount = 0;		/* current page */

    /* open this directory source file */
    if ((fp = fopen(fileName, "r")) == 0) {
	printf("fopen of %s failed : %d\n", fileName, errno);
	exit(6);
    }

    fgets(buf, READBUFSIZE, fp);	/* ignore "Alloc map:" */
    for (i = 0; i < MAXPAGES; i++) {	/* read alloMap */
	fscanf(fp, "%x", &temp);
	dhp->alloMap[i] = (unsigned char)temp;
    }
    fgets(buf, READBUFSIZE, fp);	/* ignore trailing spaces */

    fgets(buf, READBUFSIZE, fp);	/* ignore "Hash map:" */
    for (i = 0; i < NHASHENT; i++) {
	fscanf(fp, "%d", &temp);
	dhp->hashTable[i] = (unsigned short)temp;
    }
    fgets(buf, READBUFSIZE, fp);	/* ignore trailing spaces */

    while (ReadPage(fp, dhp, pgCount)) {	/* read from source file */
	if (option & verbose)
	    PrintDir(dhp, pgCount);
	if (option & update) {	/* update destnation dir inode */
	    if (pgCount == 0)	/* first page */
		if ((fd = iopen(dev, node, O_WRONLY)) < 0) {
		    printf("Error in opening destination inode %d(err %d)\n",
			   node, errno);
		    exit(1);
		}
	    if (write(fd, dirPage, sizeof(dirPage)) != sizeof(dirPage)) {
		printf("Error in writing %d th page into inode %d(err %d)\n",
		       pgCount, node, errno);
		exit(1);
	    }
	}
	pgCount++;
    }
    fclose(fp);
    close(fd);
}

			/* prints out a directory data */
PrintDir(dhp, pgCount)
     struct DirHeader *dhp;
     int pgCount;		/* current page Number */
{
    int i;
    struct DirEntry *de;
    struct PageHeader *pg;

    if (pgCount == 0) {		/* first page */
	printf("Alloc map: ");
	for (i = 0; i < MAXPAGES; i++) {	/* read alloMap */
	    if ((i % 16) == 0)
		printf("\n");
	    printf("%.2x ", dhp->alloMap[i]);
	}
	printf("\nHash table:");
	for (i = 0; i < NHASHENT; i++) {
	    if ((i % 16) == 0)
		printf("\n");
	    printf("%.2d ", dhp->hashTable[i]);
	}
	printf("\n");

	/* print page header info */
	printf("--------------- Page 0 ---------------\n");
	printf("pgcnt      :%d\n", dhp->header.pgcount);
	printf("tag        :%d\n", dhp->header.tag);
	printf("freecnt    :%d(not used)\n", dhp->header.freecount);
	printf("freebitmap :");
	for (i = 0; i < EPP / 8; i++)
	    printf("%.2x ", (unsigned char)(dhp->header.freebitmap[i]));
	printf("\n");

	/* print slots in the first page of this directory */
	de = ((struct DirPage0 *)dhp)->entry;
	for (i = DHE + 1; i < EPP;
	     i++, de = (struct DirEntry *)((char *)de + 32))
	    printf("ent %d: f=%d l=%d n=%d vn=%d vu=%d name:%s\n", i,
		   de->flag, de->length, de->next, de->fid.vnode,
		   de->fid.vunique, de->name);
    } else {
	pg = &((struct DirPage1 *)dhp)->header;	/* page header */
	de = ((struct DirPage1 *)dhp)->entry;

	/* print page header info */
	printf("--------------- Page %d ---------------\n", pgCount);
	printf("pgcnt      :%d\n", pg->pgcount);
	printf("tag        :%d\n", pg->tag);
	printf("freecnt    :%d(not used)\n", pg->freecount);
	printf("freebitmap :");
	for (i = 0; i < EPP / 8; i++)
	    printf("%.2x ", (unsigned char)(pg->freebitmap[i]));
	printf("\n");

	/* print slots in this page */
	for (i = 1; i < EPP; i++, de = (struct DirEntry *)((char *)de + 32))
	    printf("ent %d: f=%d l=%d n=%d vn=%d vu=%d name:%s\n", i,
		   de->flag, de->length, de->next, de->fid.vnode,
		   de->fid.vunique, de->name);
    }
}

/*
** Returns 0 if there are no more pages 
** Returns 1 if there are more pages to be read 
*/
ReadPage(fp, dhp, pageNo)	/* Read one page(pageNo) from file fp into dhp */
     FILE *fp;
     struct DirHeader *dhp;
     int pageNo;
{
    int pgcnt, page, freecnt, freebit[EPP / 8];
    int tag;
    char buf[READBUFSIZE];	/* read buffer */
    int start;
    int i, ent, f, l, n, vnode, unique;
    char dirName[18];
    struct DirEntry *dirEntry;
    struct PageHeader *pageHeader;

    if (fscanf(fp, "--------------- Page %d ---------------\n", &page) != 1) {
	return 0;		/* no more pages */
    }
    /* defensive check */
    if (page != pageNo) {
	printf("Wrong page: pageNo %d does not match data in file %d\n",
	       pageNo, page);
	exit(1);
    }

    if (fscanf(fp, "pgcnt      :%d", &pgcnt) != 1) {
	printf("Error in looking for pgcnt:<int> in page %d\n", pageNo);
	exit(1);
    }
    fgets(buf, READBUFSIZE, fp);	/* ignore trailing spaces */

    if (fscanf(fp, "tag        :%d", &tag) != 1) {
	printf("Error in looking for tag:<int> in page %d\n", pageNo);
	exit(1);
    }
    fgets(buf, READBUFSIZE, fp);	/* ignore trailing spaces */

    if (fscanf(fp, "freecnt    :%d", &freecnt) != 1) {
	printf("Error in looking for freecnt:<int> in page %d\n", pageNo);
	exit(1);
    }
    fgets(buf, READBUFSIZE, fp);	/* ignore trailing spaces */

    if (fscanf
	(fp, "freebitmap :%x %x %x %x %x %x %x %x", &freebit[0], &freebit[1],
	 &freebit[2], &freebit[3], &freebit[4], &freebit[5], &freebit[6],
	 &freebit[7]) != 8) {
	printf("Error in looking for freecnt:<ints> in page %d\n", pageNo);
	exit(1);
    }
    fgets(buf, READBUFSIZE, fp);	/* ignore trailing spaces */

    if (pageNo == 0) {		/* first page */
	start = DHE + 1;	/* this is 13 */
	dirEntry = ((struct DirPage0 *)dhp)->entry;
	pageHeader = &(dhp->header);
    } else {
	start = 1;
	dirEntry = ((struct DirPage1 *)dhp)->entry;
	pageHeader = &(((struct DirPage1 *)dhp)->header);
    }

    /* update page header */
    pageHeader->pgcount = pgcnt;
    pageHeader->tag = tag;
    pageHeader->freecount = freecnt;	/* this is currently unused */
    for (i = 0; i < EPP / 8; i++)
	pageHeader->freebitmap[i] = freebit[i];

    /* update directory entries */
    for (; start < EPP; start++) {
	if (fscanf
	    (fp, "ent %d: f=%d l=%d n=%d vn=%d vu=%d name:%s\n", &ent, &f, &l,
	     &n, &vnode, &unique, dirName) != 7) {
	    printf("Error in reading the %d th entry in page %d\n", start,
		   pageNo);
	    exit(1);
	}
	if (ent != start) {
	    printf("Wrong dir entry: found %d, has to be %\n", ent, start);
	    exit(1);
	}
	dirEntry->flag = f;
	dirEntry->length = l;
	dirEntry->next = n;
	dirEntry->fid.vnode = vnode;
	dirEntry->fid.vunique = unique;
	memcpy(dirEntry->name, dirName, 18);
	strncpy(dirEntry->name, dirName);

	dirEntry = (struct DirEntry *)((char *)dirEntry + 32);
    }
    return 1;			/* there are more pages */
}
