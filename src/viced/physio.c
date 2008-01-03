/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*  physio.c	- Physical I/O routines for the buffer package		*/
/*									*/
/*  Date: 5/1/85							*/
/*									*/
/************************************************************************/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#else
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <afs/nfs.h>
#include <afs/assert.h>
#include <lwp.h>
#include <lock.h>
#include <time.h>
#include <afs/afsint.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include "viced_prototypes.h"
#include "viced.h"
#ifdef PAGESIZE
#undef PAGESIZE
#endif
#define PAGESIZE 2048

afs_int32 lpErrno, lpCount;

/* returns 0 on success, errno on failure */
int
ReallyRead(DirHandle * file, int block, char *data)
{
    int code;
    FdHandle_t *fdP;

    fdP = IH_OPEN(file->dirh_handle);
    if (fdP == NULL) {
	code = errno;
	ViceLog(0,
		("ReallyRead(): open failed device %X inode %s errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(NULL,
						       file->dirh_handle->
						       ih_ino), code));
	return code;
    }
    if (FDH_SEEK(fdP, block * PAGESIZE, SEEK_SET) < 0) {
	code = errno;
	ViceLog(0,
		("ReallyRead(): lseek failed device %X inode %s errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(NULL,
						       file->dirh_handle->
						       ih_ino), code));
	FDH_REALLYCLOSE(fdP);
	return code;
    }
    code = FDH_READ(fdP, data, PAGESIZE);
    if (code != PAGESIZE) {
	if (code < 0)
	    code = errno;
	else
	    code = EIO;
	ViceLog(0,
		("ReallyRead(): read failed device %X inode %s errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(NULL,
						       file->dirh_handle->
						       ih_ino), code));
	FDH_REALLYCLOSE(fdP);
	return code;
    }
    FDH_CLOSE(fdP);
    return 0;

}

/* returns 0 on success, errno on failure */
int
ReallyWrite(DirHandle * file, int block, char *data)
{
    afs_int32 count;
    FdHandle_t *fdP;

    fdP = IH_OPEN(file->dirh_handle);
    if (fdP == NULL) {
	ViceLog(0,
		("ReallyWrite(): open failed device %X inode %s errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(NULL,
						       file->dirh_handle->
						       ih_ino), errno));
	lpErrno = errno;
	return 0;
    }
    if (FDH_SEEK(fdP, block * PAGESIZE, SEEK_SET) < 0) {
	ViceLog(0,
		("ReallyWrite(): lseek failed device %X inode %s errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(NULL,
						       file->dirh_handle->
						       ih_ino), errno));
	lpErrno = errno;
	FDH_REALLYCLOSE(fdP);
	return 0;
    }
    if ((count = FDH_WRITE(fdP, data, PAGESIZE)) != PAGESIZE) {
	ViceLog(0,
		("ReallyWrite(): write failed device %X inode %s errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(NULL,
						       file->dirh_handle->
						       ih_ino), errno));
	lpCount = count;
	lpErrno = errno;
	FDH_REALLYCLOSE(fdP);
	return 0;
    }
    FDH_CLOSE(fdP);
    return 0;
}


void
SetDirHandle(register DirHandle * dir, register Vnode * vnode)
{
    register Volume *vp = vnode->volumePtr;
    register IHandle_t *h;
    IH_COPY(h, vnode->handle);
    dir->dirh_ino = h->ih_ino;
    dir->dirh_dev = h->ih_dev;
    dir->dirh_vid = h->ih_vid;
    dir->dirh_cacheCheck = vp->cacheCheck;
    dir->dirh_unique = vnode->disk.uniquifier;
    dir->dirh_vnode = vnode->vnodeNumber;
    dir->dirh_handle = h;
}

void
FidZap(DirHandle * file)
{
    IH_RELEASE(file->dirh_handle);
    memset((char *)file, 0, sizeof(DirHandle));
}

void
FidZero(DirHandle * file)
{
    memset((char *)file, 0, sizeof(DirHandle));
}

int
FidEq(DirHandle * afile, DirHandle * bfile)
{
    if (afile->dirh_ino != bfile->dirh_ino)
	return 0;
    if (afile->dirh_dev != bfile->dirh_dev)
	return 0;
    if (afile->dirh_vid != bfile->dirh_vid)
	return 0;
    if (afile->dirh_cacheCheck != bfile->dirh_cacheCheck)
	return 0;
    if (afile->dirh_unique != bfile->dirh_unique)
	return 0;
    if (afile->dirh_vnode != bfile->dirh_vnode)
	return 0;

    return 1;
}

int
FidVolEq(DirHandle * afile, afs_int32 vid)
{
    if (afile->dirh_vid != vid)
	return 0;
    return 1;
}

int
FidCpy(DirHandle * tofile, DirHandle * fromfile)
{
    *tofile = *fromfile;
    IH_COPY(tofile->dirh_handle, fromfile->dirh_handle);
    return 0;
}
