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

RCSID
    ("$Header$");

#include <rx/xdr.h>
#include <afs/afsint.h>
#include <stdio.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#if defined(AFS_SUN5_ENV) || defined(AFS_NT40_ENV)
#include <string.h>
#else
#include <strings.h>
#endif

#include <afs/assert.h>
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

#ifdef O_LARGEFILE
#define afs_stat	stat64
#else /* !O_LARGEFILE */
#define afs_stat	stat
#endif /* !O_LARGEFILE */

/*@printflike@*/ extern void Log(const char *format, ...);


struct Lock localLock;
char *vol_DevName();

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
 * Note that ainfo->u.param[0] is always the volume ID, for any vice inode.
 */
static int
NukeProc(struct ViceInodeInfo *ainfo, afs_int32 avolid, struct ilist **allInodes)
{
    struct ilist *ti;
    register afs_int32 i;

#ifndef AFS_PTHREAD_ENV
    IOMGR_Poll();		/* poll so we don't kill the RPC connection */
#endif /* !AFS_PTHREAD_ENV */

    /* check if this is the volume we're looking for */
    if (ainfo->u.param[0] != avolid)
	return 0;		/* don't want this one */
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
    register afs_int32 code;
    int i, forceSal;
    char devName[64], wpath[100];
    char *lastDevComp;
#ifdef AFS_NAMEI_ENV
#ifdef AFS_NT40_ENV
    char path[MAX_PATH];
#else
    char *path;
    namei_t ufs_name;
#endif
#endif /* AFS_NAMEI_ENV */
    IHandle_t *fileH;
    struct ilist *allInodes = 0;

    if (avolid == 0)
	return EINVAL;
    code = afs_stat(aname, &tstat);
    if (code) {
	printf("volnuke: partition %s does not exist.\n", aname);
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
    lastDevComp = strrchr(devName, '/');
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
#ifdef AFS_NAMEI_ENV
    code =
	ListViceInodes(lastDevComp, aname, NULL, NukeProc, avolid, &forceSal,
		       0, wpath, &allInodes);
#else
    code =
	ListViceInodes(lastDevComp, aname, "/tmp/vNukeXX", NukeProc, avolid,
		       &forceSal, 0, wpath, &allInodes);
    unlink("/tmp/vNukeXX");	/* clean it up now */
#endif
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
		nt_HandleToName(path, fileH);
#else
		IH_INIT(fileH, (int)volutil_GetPartitionID(aname), avolid,
			ti->inode[i]);
		namei_HandleToName(&ufs_name, fileH);
		path = ufs_name.n_path;
#endif /* AFS_NT40_ENV */
		IH_RELEASE(fileH);
		if (unlink(path) < 0) {
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
	/* reuse devName buffer now */
#ifdef AFS_NT40_ENV
	afs_snprintf(devName, sizeof devName, "%c:\\%s", *lastDevComp,
		     VolumeExternalName(avolid));
#else
	afs_snprintf(devName, sizeof devName, "%s/%s", aname,
		     VolumeExternalName(avolid));
#endif /* AFS_NT40_ENV */
	code = unlink(devName);
	if (code)
	    code = errno;
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
