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

#define	allNull		0x00
#define allFiles	0x01	/* equivalent to /bin/ls */
#define lFlag		0x02	/* equivalent to /bin/ls -l */
#define allDirs		0x04	/* equivalent to /bin/ls -ld */
#define contentsDInode	0x08	/* list contents of dir inode */
#define volInfo		0x10	/* list info from vol header */

extern DirEnt *lookup();
extern char *getFileName(), *getDirName(), *printStack();
extern DirEnt *hash[];

int
Usage(name)
     char *name;
{
    assert(name);
    printf
	("Usage is %s -p <partition name> -v <volume name> [-ls | -lsl | -ld] [-volInfo] [-dir <directory inode>] \n",
	 name);
    printf("-ls  : lists all files\n");
    printf("-lsl : lists all files along with its properties\n");
    printf("-ld :  lists all directories\n");
    printf("-volInfo : lists volume header \n");
    printf("-dir <inode number>: prints contents of directory inode\n");
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
    int fd, i, sawPart = 0, sawVolume = 0, sawDirContents = 0;
    char option = allNull;	/* no default options */
    Inode dirInode;		/* list contents of this dir Inode */

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
	} else if (!strcmp(argv[i], "-dir")) {
	    if ((i + 1) >= argc)
		Usage(argv[0]);
	    dirInode = atoi(argv[++i]);
	    option |= contentsDInode;
	    sawDirContents = 1;
	} else if (!strcmp(argv[i], "-ls"))
	    option |= allFiles;
	else if (!strcmp(argv[i], "-lsl"))
	    option |= (allFiles | lFlag);
	else if (!strcmp(argv[i], "-ld"))
	    option |= allDirs;
	else if (!strcmp(argv[i], "-volInfo"))
	    option |= volInfo;
	else
	    Usage(argv[0]);
    }
    /* check input parameters */
    if (!sawPart || !sawVolume)
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

    switch (option) {
    case volInfo:		/* volume header info */
	printf
	    ("VolId:%d VolInfo:%d mag:%x vers:%d smallVnodeIndex:%d largeVnodeIndex:%d VoAcl:%d volMntTab:%d\n",
	     volumeHeader.id, volumeHeader.volumeInfo,
	     volumeHeader.stamp.magic, volumeHeader.stamp.version,
	     volumeHeader.smallVnodeIndex, volumeHeader.largeVnodeIndex,
	     volumeHeader.volumeAcl, volumeHeader.volumeMountTable);
	break;

    case contentsDInode:	/* list directory entries */
	printContentsOfDirInode(statBuf.st_dev, dirInode, fullName, option);
	break;
    }

    scanLargeVnode(statBuf.st_dev, volumeHeader.largeVnodeIndex, fullName,
		   option);
    if (option & allDirs)
	printDirs(fullName);

    if (option & allFiles)
	scanSmallVnode(statBuf.st_dev, volumeHeader.smallVnodeIndex, fullName,
		       option);
    close(fd);
}

int
scanLargeVnode(dev, node, partitionName, option)
     dev_t dev;
     Inode node;
     char *partitionName;
     char option;		/* user options */
{
    afs_int32 diskSize = SIZEOF_LARGEDISKVNODE;
    int nVnodes, fdi, vnodeIndex, offset = 0;
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    FILE *file;
    struct stat statBuf;

    /* open this largeVodeIndex */
    if ((fdi = iopen(dev, node, O_RDONLY)) < 0) {
	printf("Error in reading node : %d\n", errno);
	exit(5);
    }

    /* get a FILE pointer */
    if ((file = fdopen(fdi, "r")) == 0) {
	printf("fdopen failed : %d\n", errno);
	exit(6);
    }

    /*find out how many directories are there in this volume */
    if (fstat(fdi, &statBuf) < 0) {
	printf("Error in stat(fd=%d): %d\n", fdi, errno);
	exit(6);
    }
    nVnodes = (statBuf.st_size / diskSize) - 1;
    if (nVnodes > 0)
	fseek(file, diskSize, 0);
    else
	nVnodes = 0;

    /* scan all entries in this volume */
    DInit(10);			/* initialise directory buffer */

    for (vnodeIndex = 0; nVnodes && fread(vnode, diskSize, 1, file) == 1;
	 nVnodes--, vnodeIndex++, offset += diskSize) {
	/* scan this directory */
	int createDirEnt();
	if ((vnode->type == vDirectory) && (vnode->inodeNumber)) {
	    DirHandle dir;
	    DirEnt *dirEntry;

	    dir.volume = volumeId;
	    dir.device = dev;
	    dir.cacheCheck = 0;
	    dir.inode = vnode->inodeNumber;
#ifdef	DEBUG
	    printf
		("Directory inode %d (parent vnode = %d) contains the entries :\n",
		 vnode->inodeNumber, vnode->parent);
#endif

	    assert(dirEntry = (DirEnt *) malloc(sizeof(DirEnt)));
	    dirEntry->inode = vnode->inodeNumber;
	    dirEntry->numEntries = 0;
	    dirEntry->vnodeName = NULL;
	    EnumerateDir(&dir, &createDirEnt, dirEntry);
	    insertHash(dirEntry);	/* insert in hash table */
	}
    }
    fclose(file);
#ifdef DEBUG
    printHash();
#endif
}



int
createDirEnt(dirEntry, fileName, vnode, unique)
     DirEnt *dirEntry;
     char *fileName;
     afs_int32 vnode;
     afs_int32 unique;
{
    int fdi;
    FILE *file;
    struct stat statBuf;

    /* fil up special fields for itself and parent */
    if (strcmp(fileName, ".") == 0) {
	dirEntry->vnode = vnode;
	return;
    }
    if (strcmp(fileName, "..") == 0) {
	dirEntry->vnodeParent = vnode;
	return;
    }

    (dirEntry->numEntries)++;
    assert(dirEntry->vnodeName =
	   (VnodeName *) realloc(dirEntry->vnodeName,
				 dirEntry->numEntries * sizeof(VnodeName)));
    dirEntry->vnodeName[dirEntry->numEntries - 1].vnode = vnode;
    dirEntry->vnodeName[dirEntry->numEntries - 1].vunique = unique;
    dirEntry->vnodeName[dirEntry->numEntries - 1].name =
	(char *)malloc(strlen(fileName) + 1);
    assert(dirEntry->vnodeName[dirEntry->numEntries - 1].name);
    strcpy(dirEntry->vnodeName[dirEntry->numEntries - 1].name, fileName);
}


int
scanSmallVnode(dev, node, partitionName, option)
     dev_t dev;
     Inode node;
     char *partitionName;
     char option;		/* user options */
{
    afs_int32 diskSize = SIZEOF_SMALLDISKVNODE;
    int nVnodes, fdi, vnodeIndex, offset = 0;
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    FILE *file;
    struct stat statBuf;

    /* open this smallVodeIndex */
    if ((fdi = iopen(dev, node, O_RDONLY)) < 0) {
	printf("Error in reading node : %d\n", errno);
	exit(5);
    }

    /* get a FILE pointer */
    if ((file = fdopen(fdi, "r")) == 0) {
	printf("fdopen failed : %d\n", errno);
	exit(6);
    }

    /*find out how many files are there in this volume */
    if (fstat(fdi, &statBuf) < 0) {
	printf("Error in stat(fd=%d): %d\n", fdi, errno);
	exit(6);
    }
    nVnodes = (statBuf.st_size / diskSize) - 1;
    if (nVnodes > 0)
	fseek(file, diskSize, 0);
    else
	nVnodes = 0;

    /* scan all entries in this volume */
    for (vnodeIndex = 0; nVnodes && fread(vnode, diskSize, 1, file) == 1;
	 nVnodes--, vnodeIndex++, offset += 1)
	if ((vnode->type == vFile) || (vnode->type == vSymlink)) {
	    char *name, *fullPathName;
	    int pNode, nNode, orphan = 0;
	    DirEnt *dir;
#ifdef FILE_DEBUG
	    printf(" File Inode = %d parent vnode=%d ", vnode->inodeNumber,
		   vnode->parent);
#endif

	    if ((dir = lookup(vnode->parent)) == 0)	/* orphaned */
		orphan = 1, pushStack(orphan_NoVnode);
	    if (!orphan) {
		name = getFileName(dir, vnode->uniquifier);
		if (name == 0)
		    orphan = 1, pushStack(orphan_NoUnique);
	    }
	    if (!orphan) {
		/* push the file name on stack */
		pushStack(name);
		pNode = vnode->parent;
		nNode = dir->vnodeParent;
	    }
	    while (!orphan && (pNode != nNode)) {
		if ((dir = lookup(nNode)) == 0) {
		    orphan = 1, pushStack(orphan_NoVnode);
		    break;
		}
		if ((name = getDirName(dir, pNode)) == 0) {
		    orphan = 1, pushStack(orphan_NoVnode);
		    break;
		}
		pushStack(name);
		pNode = nNode;
		nNode = dir->vnodeParent;
	    }
	    fullPathName = printStack();	/* full name of file or symLink */
	    if (vnode->type == vSymlink) {	/* check if mount point */
		/* read contents of link */
		struct stat statLink;
		int fdLink;
		char *symLink;
		if ((fdLink = iopen(dev, vnode->inodeNumber, O_RDONLY)) < 0) {
		    printf("Error in opening symbolic link : %d\n", errno);
		    exit(10);
		}
		if (fstat(fdLink, &statLink) < 0) {
		    printf("Error in symLink stat(fd=%d): %d\n", fdLink,
			   errno);
		    exit(12);
		}
		assert(symLink = (char *)malloc(statLink.st_size + 1));
		if (read(fdLink, symLink, statLink.st_size) < 0) {
		    printf("Error in reading symbolic link : %d\n", errno);
		    exit(11);
		}
		symLink[statLink.st_size] = 0;	/* null termination */
		if (symLink[0] == '#')	/* this is a mount point */
		    printf("Volume %s mounted on %s%s\n", symLink,
			   partitionName, fullPathName);
		free(symLink);
		close(fdLink);
	    }
	    if (option & allFiles) {
		if (option & lFlag) {
		    switch (vnode->type) {
		    case vFile:
			printf("F ");
			break;
		    case vDirectory:
			printf("D ");
			break;
		    case vSymlink:
			printf("S ");
			break;
		    default:
			printf("U ");
			break;
		    }
		    printf("Ind:%d ", vnode->inodeNumber);
		    printf("Mod:%x ", vnode->modeBits);
		    printf("Lnk:%d ", vnode->linkCount);
		    printf("Own:%d ", vnode->owner);
		    printf("Grp:%d ", vnode->group);
		    printf("Siz:%d ", vnode->length);
		}
		printf("~%s\n", fullPathName);
	    }
	}

    fclose(file);
}

/* Lists all directories in the volume */
printDirs(partitionName)
     char *partitionName;
{
    int i, j, vnode, inode;
    DirEnt *ptr, *dir, *tmpDir;

    /* The root level vnode for this volume */
    tmpDir = lookup(1);		/* root vnode is 1 */
    if (tmpDir == 0)
	printf("Root vnode(1) does not exists :%s\n", partitionName);
    else
	printf("D Ind:%d Vnd:1 ~\n", tmpDir->inode);

    for (i = 0; i < MAX_HASH_SIZE; i++)
	for (ptr = (DirEnt *) hash[i]; ptr; ptr = ptr->next)
	    for (j = 0; j < ptr->numEntries; j++) {
		int nVnode, pVnode;
		char *fullPathName, *name;

		pVnode = ptr->vnodeParent;	/* parent vnode */
		nVnode = ptr->vnode;	/* this dir vnode */
		vnode = ptr->vnodeName[j].vnode;	/* my Vnode */

		/* directory vnode numbers are odd */
		if ((vnode % 2) == 0)
		    continue;

		tmpDir = lookup(vnode);
		if (!tmpDir) {	/* orphaned directory */
		    printf("%s : vnode:%d \n", orphan_NoVnode, vnode);
		    continue;
		}
		inode = tmpDir->inode;	/* the inode for this vnode */

		pushStack(ptr->vnodeName[j].name);

		while (pVnode != 1) {
		    dir = lookup(pVnode);
		    if (dir == 0) {	/* orphan */
			pushStack(orphan_NoVnode);
			break;
		    }
		    name = getDirName(dir, nVnode);
		    if (name == 0) {
			pushStack(orphan_NoVnode);
			break;
		    }
		    pushStack(name);
		    nVnode = pVnode;
		    pVnode = dir->vnodeParent;
		}
		fullPathName = printStack();	/* full path name of directory */
		printf("D Ind:%d Vnd:%d ~%s\n", inode, vnode, fullPathName);
	    }
}

/* figure out how many pages in use in a directory, given ptr to its (locked) he
ader */
static
ComputeUsedPages(dhp)
     register struct DirHeader *dhp;
{
    register afs_int32 usedPages, i;

    if (dhp->header.pgcount != 0) {
	/* new style */
	usedPages = ntohs(dhp->header.pgcount);
    } else {
	/* old style */
	usedPages = 0;
	for (i = 0; i < MAXPAGES; i++) {
	    if (dhp->alloMap[i] == EPP) {
		usedPages = i;
		break;
	    }
	}
	if (usedPages == 0)
	    usedPages = MAXPAGES;
    }
    return usedPages;
}

printContentsOfDirInode(device, dirInode, fullName, options)
     dev_t device;
     Inode dirInode;
     char *fullName;
     char options;
{
    int fd, i, j, usedPages, pages;
    FILE *file;
    struct stat statBuf;
    char dirPage[2048];
    struct DirHeader *dhp = (struct DirHeader *)&dirPage[0];
    struct DirEntry *de;
    struct PageHeader *pg;

    fd = iopen(device, dirInode, O_RDONLY);
    if (fd <= 0) {
	printf("Cannot open direcory inode %d\n", dirInode);
	return -1;
    }
    if ((file = fdopen(fd, "r")) == 0) {	/* for buffered read */
	printf("fdopen failed : %d\n", errno);
	close(fd);
	return -1;
    }
    if (fstat(fd, &statBuf) < 0) {
	printf("Error in stat(fd=%d): %d\n", fd, errno);
	return -1;
    }
    /* read first page */
    if (fread(&dirPage, sizeof(dirPage), 1, file) != 1) {
	printf("Cannot read dir header from inode %d(errno %d)\n", dirInode,
	       errno);
	fclose(file);
	return -1;
    }
    usedPages = ComputeUsedPages(dhp);

    printf("Alloc map: ");
    for (i = 0; i < MAXPAGES; i++) {
	if ((i % 16) == 0)
	    printf("\n");
	printf("%.2x ", (unsigned char)dhp->alloMap[i]);
    }
    printf("\nHash table:");
    for (i = 0; i < NHASHENT; i++) {
	if ((i % 16) == 0)
	    printf("\n");
	printf("%.2d ", dhp->hashTable[i]);
    }
    printf("\n");

    /* print header of first page */
    printf("--------------- Page 0 ---------------\n");
    printf("pgcnt      :%d\n", usedPages);
    printf("tag        :%d\n", dhp->header.tag);
    printf("freecnt    :%d(not used)\n", dhp->header.freecount);
    printf("freebitmap :");
    for (i = 0; i < EPP / 8; i++)
	printf("%.2x ", (unsigned char)(dhp->header.freebitmap[i]));
    printf("\n");

    /* print slots in the first page of this directory */
    de = ((struct DirPage0 *)dirPage)->entry;
    for (i = DHE + 1; i < EPP; i++, de = (struct DirEntry *)((char *)de + 32))
	printf("ent %d: f=%d l=%d n=%d vn=%d vu=%d name:%s\n", i, de->flag,
	       de->length, de->next, de->fid.vnode, de->fid.vunique,
	       de->name);

    /* read all succeeding pages of this directory */
    for (pages = 1; pages < usedPages; pages++) {
	if (fread(&dirPage, sizeof(dirPage), 1, file) != 1) {
	    printf("Cannot read %s page  from inode %d(errno %d)\n", pages,
		   dirInode, errno);
	    fclose(file);
	    return -1;
	}
	pg = &((struct DirPage1 *)dirPage)->header;	/* page header */
	de = ((struct DirPage1 *)dirPage)->entry;

	/* print page header info */
	printf("--------------- Page %d ---------------\n", pages);
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

    fclose(file);
    return 0;
}
