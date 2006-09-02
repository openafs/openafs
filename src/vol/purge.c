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
	Module:		purge.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/vol/purge.c,v 1.9.2.2 2006/07/13 18:20:32 shadow Exp $");

#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <io.h>
#else
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <sys/stat.h>
#include <afs/assert.h>

#include <rx/xdr.h>
#include "afs/afsint.h"
#include "nfs.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#ifdef AFS_NT40_ENV
#include "ntops.h"
#endif
#include "vnode.h"
#include "volume.h"
#include "viceinode.h"
#include "partition.h"
#include "fssync.h"

/* forward declarations */
void PurgeIndex_r(Volume * vp, VnodeClass class);
void PurgeHeader_r(Volume * vp);

/* No lock needed. Only the volserver will call this, and only one transaction
 * can have a given volume (volid/partition pair) in use at a time 
 */
void
VPurgeVolume(Error * ec, Volume * vp)
{
    struct DiskPartition *tpartp = vp->partition;
    char purgePath[MAXPATHLEN];

    /* N.B.  it's important here to use the partition pointed to by the
     * volume header. This routine can, under some circumstances, be called
     * when two volumes with the same id exist on different partitions.
     */
    (void)afs_snprintf(purgePath, sizeof purgePath, "%s/%s",
		       VPartitionPath(vp->partition),
		       VolumeExternalName(V_id(vp)));
    PurgeIndex_r(vp, vLarge);
    PurgeIndex_r(vp, vSmall);
    PurgeHeader_r(vp);
    unlink(purgePath);
    /*
     * Call the fileserver to break all call backs for that volume
     */
    FSYNC_askfs(V_id(vp), tpartp->name, FSYNC_RESTOREVOLUME, 0);
}

#define MAXOBLITATONCE	200
/* delete a portion of an index, adjusting offset appropriately.  Returns 0 if
   things work and we should be called again, 1 if success full and done, and -1
   if an error occurred.  It adjusts offset appropriately on 0 or 1 return codes,
   and otherwise doesn't touch it */
static int
ObliterateRegion(Volume * avp, VnodeClass aclass, StreamHandle_t * afile,
		 afs_int32 * aoffset)
{
    register struct VnodeClassInfo *vcp;
    Inode inodes[MAXOBLITATONCE];
    register afs_int32 iindex, nscanned;
    afs_int32 offset;
    char buf[SIZEOF_LARGEDISKVNODE];
    int hitEOF;
    register int i;
    register afs_int32 code;
    register struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;

    hitEOF = 0;
    vcp = &VnodeClassInfo[aclass];
    offset = *aoffset;		/* original offset */
    iindex = 0;
    nscanned = 0;
    /* advance over up to MAXOBLITATONCE inodes.  nscanned tells us how many we examined.
     * We remember the inodes in an array, and idec them after zeroing them in the index.
     * The reason for these contortions is to make volume deletion idempotent, even
     * if we crash in the middle of a delete operation. */
    STREAM_SEEK(afile, offset, 0);
    while (1) {
	if (iindex >= MAXOBLITATONCE) {
	    break;
	}
	code = STREAM_READ(vnode, vcp->diskSize, 1, afile);
	nscanned++;
	offset += vcp->diskSize;
	if (code != 1) {
	    hitEOF = 1;
	    break;
	}
	if (vnode->type != vNull) {
	    if (vnode->vnodeMagic != vcp->magic)
		goto fail;	/* something really wrong; let salvager take care of it */
	    if (VNDISK_GET_INO(vnode))
		inodes[iindex++] = VNDISK_GET_INO(vnode);
	}
    }

    /* next, obliterate the index and fflush (and fsync) it */
    STREAM_SEEK(afile, *aoffset, 0);	/* seek back to start of vnode index region */
    memset(buf, 0, sizeof(buf));	/* zero out our proto-vnode */
    for (i = 0; i < nscanned; i++) {
	if (STREAM_WRITE(buf, vcp->diskSize, 1, afile) != 1)
	    goto fail;
    }
    STREAM_FLUSH(afile);	/* ensure 0s are on the disk */
    OS_SYNC(afile->str_fd);

    /* finally, do the idec's */
    for (i = 0; i < iindex; i++) {
	IH_DEC(V_linkHandle(avp), inodes[i], V_parentId(avp));
	DOPOLL;
    }

    /* return the new offset */
    *aoffset = offset;
    return hitEOF;		/* return 1 if hit EOF (don't call again), otherwise 0 */

  fail:
    return -1;
}

void
PurgeIndex(Volume * vp, VnodeClass class)
{
    VOL_LOCK;
    PurgeIndex_r(vp, class);
    VOL_UNLOCK;
}

void
PurgeIndex_r(Volume * vp, VnodeClass class)
{
    StreamHandle_t *ifile;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    afs_int32 offset;
    register afs_int32 code;
    FdHandle_t *fdP;


    fdP = IH_OPEN(vp->vnodeIndex[class].handle);
    if (fdP == NULL)
	return;

    ifile = FDH_FDOPEN(fdP, "r+");
    if (!ifile) {
	FDH_REALLYCLOSE(fdP);
	return;
    }

    offset = vcp->diskSize;
    while (1) {
	code = ObliterateRegion(vp, class, ifile, &offset);
	if (code)
	    break;		/* if error or hit EOF */
    }
    STREAM_CLOSE(ifile);
    FDH_CLOSE(fdP);
}

void
PurgeHeader(Volume * vp)
{
    VOL_LOCK;
    PurgeHeader_r(vp);
    VOL_UNLOCK;
}

void
PurgeHeader_r(Volume * vp)
{
    IH_REALLYCLOSE(V_diskDataHandle(vp));
    IH_DEC(V_linkHandle(vp), vp->vnodeIndex[vLarge].handle->ih_ino, V_id(vp));
    IH_DEC(V_linkHandle(vp), vp->vnodeIndex[vSmall].handle->ih_ino, V_id(vp));
    IH_DEC(V_linkHandle(vp), vp->diskDataHandle->ih_ino, V_id(vp));
#ifdef AFS_NAMEI_ENV
    /* And last, but not least, the link count table itself. */
    IH_REALLYCLOSE(V_linkHandle(vp));
    IH_DEC(V_linkHandle(vp), vp->linkHandle->ih_ino, V_parentId(vp));
#endif
}
