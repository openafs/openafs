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

#include <roken.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include <rx/rx_queue.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <afs/afs_lock.h>
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

/*
 * Read the specified page from a directory object
 *
 * \parm[in] file      handle to the directory object
 * \parm[in] block     requested page from the directory object
 * \parm[out] data     buffer for the returned page
 * \parm[out] physerr  (optional) pointer to errno if physical error
 *
 * \retval 0   success
 * \retval EIO physical or logical error;
 *             if physerr is supplied by caller, it will be set to:
 *                 0       for logical errors
 *                 errno   for physical errors
 */
int
ReallyRead(DirHandle *file, int block, char *data, int *physerr)
{
    int saverr = 0;
    int code = 0;
    ssize_t rdlen;
    FdHandle_t *fdP;
    afs_ino_str_t stmp;

    fdP = IH_OPEN(file->dirh_handle);
    if (fdP == NULL) {
	saverr = errno;
	code = EIO;
	ViceLog(0,
		("ReallyRead(): open failed device %X inode %s (volume=%" AFS_VOLID_FMT ") errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(stmp,
						       file->dirh_handle->
						       ih_ino), 
		 afs_printable_VolumeId_lu(file->dirh_handle->ih_vid), saverr));
	goto done;
    }
    rdlen = FDH_PREAD(fdP, data, PAGESIZE, ((afs_foff_t)block) * PAGESIZE);
    if (rdlen != PAGESIZE) {
	if (rdlen < 0)
	    saverr = errno;
	else
	    saverr = 0;	    /* logical error: short read */
	code = EIO;
	ViceLog(0,
		("ReallyRead(): read failed device %X inode %s (volume=%" AFS_VOLID_FMT ") errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(stmp,
						       file->dirh_handle->
						       ih_ino),
		 afs_printable_VolumeId_lu(file->dirh_handle->ih_vid), saverr));
	FDH_REALLYCLOSE(fdP);
	goto done;
    }
    FDH_CLOSE(fdP);

 done:
    if (physerr != NULL)
	*physerr = saverr;
    return code;

}

/* returns 0 on success, errno on failure */
int
ReallyWrite(DirHandle * file, int block, char *data)
{
    ssize_t count;
    FdHandle_t *fdP;
    afs_ino_str_t stmp;

    fdP = IH_OPEN(file->dirh_handle);
    if (fdP == NULL) {
	ViceLog(0,
		("ReallyWrite(): open failed device %X inode %s (volume=%" AFS_VOLID_FMT ") errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(stmp,
						       file->dirh_handle->
						       ih_ino),
		 afs_printable_VolumeId_lu(file->dirh_handle->ih_vid), errno));
	lpErrno = errno;
	return 0;
    }
    if ((count = FDH_PWRITE(fdP, data, PAGESIZE, ((afs_foff_t)block) * PAGESIZE)) != PAGESIZE) {
	ViceLog(0,
		("ReallyWrite(): write failed device %X inode %s (volume=%" AFS_VOLID_FMT ") errno %d\n",
		 file->dirh_handle->ih_dev, PrintInode(stmp,
						       file->dirh_handle->
						       ih_ino),
		 afs_printable_VolumeId_lu(file->dirh_handle->ih_vid), errno));
	lpCount = count;
	lpErrno = errno;
	FDH_REALLYCLOSE(fdP);
	return 0;
    }
    FDH_CLOSE(fdP);
    return 0;
}


void
SetDirHandle(DirHandle * dir, Vnode * vnode)
{
    Volume *vp = vnode->volumePtr;
    IHandle_t *h;
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
    memset(file, 0, sizeof(DirHandle));
}

void
FidZero(DirHandle * file)
{
    memset(file, 0, sizeof(DirHandle));
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
FidVolEq(DirHandle * afile, VolumeId vid)
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
