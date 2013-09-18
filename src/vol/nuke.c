/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <rx/xdr.h>
#include <afs/afsint.h>
#include <stdio.h>
#include <afs/afs_assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#if defined(AFS_SUN5_ENV) || defined(AFS_NT40_ENV)
#include <string.h>
#else
#include <strings.h>
#endif
#ifndef AFS_NT40_ENV
#include <unistd.h>
#endif

#include <afs/afsutil.h>

#include "nfs.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include "viceinode.h"
#include "salvage.h"
#include "daemon_com.h"
#include "fssync.h"
#include "common.h"

#ifdef O_LARGEFILE
#define afs_stat	stat64
#else /* !O_LARGEFILE */
#define afs_stat	stat
#endif /* !O_LARGEFILE */

struct Lock localLock;

#define MAXATONCE	100
/* structure containing neatly packed set of inodes and the # of times we'll have
 * to idec them in order to reclaim their storage.  NukeProc, called by ListViceInodes,
 * builds this list for us.
 */
struct ilist {
    struct ilist *next;
    afs_int32 freePtr;		/* first free index in this table */
    Inode inode[MAXATONCE];	/* inode # */
    afs_int32 count[MAXATONCE];	/* link count */
};

/* called with a structure specifying info about the inode, and our rock (which
 * is the volume ID.  Returns true if we should keep this inode, otherwise false.
 */
static int
NukeProc(struct ViceInodeInfo *ainfo, afs_uint32 avolid, void *arock)
{
    struct ilist **allInodes = (struct ilist **)arock;
    struct ilist *ti;
    afs_int32 i;

#ifndef AFS_PTHREAD_ENV
    IOMGR_Poll();		/* poll so we don't kill the RPC connection */
#endif /* !AFS_PTHREAD_ENV */

    /* check if this is the volume we're looking for */
    if (ainfo->u.vnode.vnodeNumber == INODESPECIAL) {
	/* For special inodes, look at both the volume id and the parent id.
	 * If we were given an RW vol id to nuke, we should delete the special
	 * inodes for all volumes in the VG, since we're deleting all of the
	 * regular inodes, too. If we don't do this, on namei would be
	 * impossible to nuke the special inodes for a non-RW volume. */
	if (ainfo->u.special.volumeId != avolid && ainfo->u.special.parentId != avolid) {
	    return 0;
	}
    } else {
	if (ainfo->u.vnode.volumeId != avolid) {
	    return 0;		/* don't want this one */
	}
    }

    /* record the info */
    if (!*allInodes || (*allInodes)->freePtr >= MAXATONCE) {
	ti = (struct ilist *)malloc(sizeof(struct ilist));
	memset(ti, 0, sizeof(*ti));
	ti->next = *allInodes;
	*allInodes = ti;
    } else
	ti = *allInodes;		/* use the one with space */
    i = ti->freePtr++;		/* find our slot in this mess */
    ti->inode[i] = ainfo->inodeNumber;
    ti->count[i] = ainfo->linkCount;
    return 0;			/* don't care if anything's written out, actually */
}

/* function called with partition name and volid ID, and which removes all
 * inodes marked with the specified volume ID.  If the volume is a read-only
 * clone, we'll only remove the header inodes, since they're the only inodes
 * marked with that volume ID.  If you want to reclaim all the data, you should
 * nuke the read-write volume ID.
 *
 * Note also that nuking a read-write volume effectively nukes all RO volumes
 * cloned from that RW volume ID, too, since everything except for their
 * indices will be gone.
 */
int
nuke(char *aname, afs_int32 avolid)
{
    /* first process the partition containing this junk */
    struct afs_stat tstat;
    struct ilist *ti, *ni, *li=NULL;
    afs_int32 code;
    int i, forceSal;
    char wpath[100];
    char *lastDevComp;
    struct DiskPartition64 *dp;
#ifdef AFS_NAMEI_ENV
    char *path;

    namei_t ufs_name;
#endif /* AFS_NAMEI_ENV */
#ifndef AFS_NAMEI_ENV
    char devName[64];
#endif /* !AFS_NAMEI_ENV */
    IHandle_t *fileH;
    struct ilist *allInodes = 0;

    if (avolid == 0)
	return EINVAL;
    code = afs_stat(aname, &tstat);
    if (code || (dp = VGetPartition(aname, 0)) == NULL) {
	printf("volnuke: partition %s does not exist.\n", aname);
	if (!code) {
	    code = EINVAL;
	}
	return code;
    }
    /* get the device name for the partition */
#if defined(AFS_NAMEI_ENV) && !defined(AFS_NT40_ENV)
    lastDevComp = aname;
#else
#ifdef AFS_NT40_ENV
    lastDevComp = &aname[strlen(aname) - 1];
    *lastDevComp = toupper(*lastDevComp);
#else
    {
	char *tfile = vol_DevName(tstat.st_dev, wpath);
	if (!tfile) {
	    printf("volnuke: can't find %s's device.\n", aname);
	    return 1;
	}
	strcpy(devName, tfile);	/* save this from the static buffer */
    }
    /* aim lastDevComp at the 'foo' of '/dev/foo' */
    lastDevComp = strrchr(devName, OS_DIRSEPC);
    /* either points at slash, or there is no slash; adjust appropriately */
    if (lastDevComp)
	lastDevComp++;
    else
	lastDevComp = devName;
#endif /* AFS_NT40_ENV */
#endif /* AFS_NAMEI_ENV && !AFS_NT40_ENV */

    ObtainWriteLock(&localLock);
    /* OK, we have the mounted on place, aname, the device name (in devName).
     * all we need to do to call ListViceInodes is find the inodes for the
     * volume we're nuking.
     */
    code =
	ListViceInodes(lastDevComp, aname, NULL, NukeProc, avolid, &forceSal,
		       0, wpath, &allInodes);
    if (code == 0) {
	/* actually do the idecs now */
	for (ti = allInodes; ti; ti = ti->next) {
	    for (i = 0; i < ti->freePtr; i++) {
#ifndef AFS_PTHREAD_ENV
		IOMGR_Poll();	/* keep RPC running */
#endif /* !AFS_PTHREAD_ENV */
		/* idec this inode into oblivion */
#ifdef AFS_NAMEI_ENV
#ifdef AFS_NT40_ENV
		IH_INIT(fileH, (int)(*lastDevComp - 'A'), avolid,
			ti->inode[i]);
#else
		IH_INIT(fileH, (int)volutil_GetPartitionID(aname), avolid,
			ti->inode[i]);
#endif /* AFS_NT40_ENV */
		namei_HandleToName(&ufs_name, fileH);
		path = ufs_name.n_path;
		IH_RELEASE(fileH);
		if (OS_UNLINK(path) < 0) {
		    Log("Nuke: Failed to remove %s\n", path);
		}
#else /* AFS_NAMEI_ENV */
		IH_INIT(fileH, (int)tstat.st_dev, avolid, ti->inode[i]);
		{
		    int j;
		    for (j = 0; j < ti->count[i]; j++) {
			code = IH_DEC(fileH, ti->inode[i], avolid);
		    }
		}
		IH_RELEASE(fileH);
#endif /* AFS_NAMEI_ENV */
	    }
	    ni = ti->next;
	    if (li) free(li);
	    li = ti;
	}
	if (li) free(li);
	code = 0;		/* we really don't care about it except for debugging */
	allInodes = NULL;

	/* at this point, we should try to remove the volume header file itself.
	 * the volume header file is the file named VNNNNN.vol in the UFS file
	 * system, and is a normal file.  As such, it is not stamped with the
	 * volume's ID in its inode, and has to be removed explicitly.
	 */
	code = VDestroyVolumeDiskHeader(dp, avolid, 0);
    } else {
	/* just free things */
	for (ti = allInodes; ti; ti = ni) {
	    ni = ti->next;
	    if (li) free(li);
	    li = ti;
	}
	if (li) free(li);
	allInodes = NULL;
    }
    ReleaseWriteLock(&localLock);
    return code;
}
