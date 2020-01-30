/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		VICE-TWO
	Module:		physio.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include <rx/xdr.h>
#include <afs/afsint.h>
#include <afs/afssyscalls.h>
#include "nfs.h"
#include "ihandle.h"
#include "salvage.h"
#include <afs/dir.h>
#include "vol_internal.h"

/*
 * Read the specified page from a directory object
 *
 * \parm[in] file	handle to the directory object
 * \parm[in] block	requested page from the directory object
 * \parm[out] data	buffer for the returned page
 * \parm[out] physerr	(optional) pointer to errno if physical error
 *
 * \retval 0	success
 * \retval EIO	physical or logical error;
 *		if physerr is supplied by caller, it will be set to:
 *		    0	    for logical errors
 *		    errno   for physical errors
 */
int
ReallyRead(DirHandle *file, int block, char *data, int *physerr)
{
    FdHandle_t *fdP;
    int code = 0;
    int saverr = 0;
    ssize_t nBytes;

    errno = 0;
    fdP = IH_OPEN(file->dirh_handle);
    if (fdP == NULL) {
	saverr = errno;
	code = EIO;
	goto done;
    }
    nBytes = FDH_PREAD(fdP, data, (afs_fsize_t) AFS_PAGESIZE,
                       ((afs_foff_t)block) * AFS_PAGESIZE);
    if (nBytes != AFS_PAGESIZE) {
	if (nBytes < 0)
	    saverr = errno;
	else
	    saverr = 0;	    /* logical error: short read */
	code = EIO;
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
    FdHandle_t *fdP;
    int code;
    ssize_t nBytes;

    errno = 0;

    fdP = IH_OPEN(file->dirh_handle);
    if (fdP == NULL) {
	code = errno;
	return code;
    }
    nBytes = FDH_PWRITE(fdP, data, (afs_fsize_t) AFS_PAGESIZE,
                        ((afs_foff_t)block) * AFS_PAGESIZE);
    if (nBytes != AFS_PAGESIZE) {
	if (nBytes < 0)
	    code = errno;
	else
	    code = EIO;
	FDH_REALLYCLOSE(fdP);
	return code;
    }
    FDH_CLOSE(fdP);
    *(file->volumeChanged) = 1;
    return 0;
}

/* SetSalvageDirHandle:
 * Create a handle to a directory entry and reference it (IH_INIT).
 * The handle needs to be dereferenced with the FidZap() routine.
 */
void
SetSalvageDirHandle(DirHandle * dir, VolumeId volume, Device device,
		    Inode inode, int *volumeChanged)
{
    static int SalvageCacheCheck = 1;
    memset(dir, 0, sizeof(DirHandle));

    dir->dirh_device = device;
    dir->dirh_volume = volume;
    dir->dirh_inode = inode;
    IH_INIT(dir->dirh_handle, device, volume, inode);
    /* Always re-read for a new dirhandle */
    dir->dirh_cacheCheck = SalvageCacheCheck++;
    dir->volumeChanged = volumeChanged;
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
    if (afile->dirh_volume != bfile->dirh_volume)
	return 0;
    if (afile->dirh_device != bfile->dirh_device)
	return 0;
    if (afile->dirh_cacheCheck != bfile->dirh_cacheCheck)
	return 0;
    if (afile->dirh_inode != bfile->dirh_inode)
	return 0;
    return 1;
}

int
FidVolEq(DirHandle * afile, VolumeId vid)
{
    if (afile->dirh_volume != vid)
	return 0;
    return 1;
}

void
FidCpy(DirHandle * tofile, DirHandle * fromfile)
{
    *tofile = *fromfile;
    IH_COPY(tofile->dirh_handle, fromfile->dirh_handle);
}

void
Die(const char *msg)
{
    printf("%s\n", msg);
    osi_Panic("%s\n", msg);
}
