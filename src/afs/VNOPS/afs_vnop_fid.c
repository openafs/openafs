/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * afs_fid
 *
 */

#include <afsconfig.h>
#include "afs/param.h"


#if !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_OBSD_ENV) && !defined(AFS_NBSD_ENV)
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"



int afs_fid_vnodeoverflow = 0, afs_fid_uniqueoverflow = 0;

/*
 *  afs_fid
 * 
 * afs_fid can return two flavors of NFS fid, depending on if submounts are
 * allowed. The reason for this is that we can't guarantee that we found all 
 * the entry points any OS might use to get the fid for the NFS mountd.
 * Hence we return a "magic" fid for all but /afs. If it goes through the
 * translator code, it will get transformed into a SmallFid that we recognize.
 * So, if submounts are disallowed, and an NFS client tries a submount, it will
 * get a fid which we don't recognize and the mount will either fail or we
 * will ignore subsequent requests for that mount.
 *
 * The Alpha fid is organized differently than for other platforms. Their
 * intention was to have the data portion of the fid aligned on a 4 byte
 * boundary. To do so, the fid is organized as:
 * u_short reserved
 * u_short len
 * char data[8]
 * The len field is the length of the entire fid, from reserved through data.
 * This length is used by fid_copy to include copying the reserved field. 
 * Alpha's zero the reserved field before handing us the fid, but they use
 * it in fid_cmp. We use the reserved field to store the 16 bits of the Vnode.
 *
 * Note that the SmallFid only allows for 8 bits of the cell index and
 * 16 bits of the vnode. 
 */

#ifdef AFS_AIX41_ENV
int afs_iauth_initd = 0;
#define USE_SMALLFID(C) (afs_iauth_initd && AFS_NFSXLATORREQ(C))
#endif


extern int afs_NFSRootOnly;	/* 1 => only allow NFS mounts of /afs. */

int
#ifdef AFS_AIX41_ENV
afs_fid(OSI_VC_DECL(avc), struct fid *fidpp, struct ucred *credp)
#elif defined(AFS_SUN5_ENV)
afs_fid(OSI_VC_DECL(avc), struct fid *fidpp)
#else
afs_fid(OSI_VC_DECL(avc), struct fid **fidpp)
#endif				/* AFS_AIX41_ENV */
{
    struct SmallFid Sfid;
    long addr[2];
    struct cell *tcell;
    extern struct vcache *afs_globalVp;
    int SizeOfSmallFid = SIZEOF_SMALLFID;
    int rootvp = 0;
    OSI_VC_CONVERT(avc);

    AFS_STATCNT(afs_fid);

    if (afs_shuttingdown != AFS_RUNNING)
	return EIO;

    if (afs_NFSRootOnly && (avc == afs_globalVp))
	rootvp = 1;
    if (!afs_NFSRootOnly || rootvp
#ifdef AFS_AIX41_ENV
	|| USE_SMALLFID(credp)
#endif
	) {
	tcell = afs_GetCell(avc->f.fid.Cell, READ_LOCK);
	Sfid.Volume = avc->f.fid.Fid.Volume;
	Sfid.Vnode = avc->f.fid.Fid.Vnode;
	Sfid.CellAndUnique =
	    ((tcell->cellIndex << 24) + (avc->f.fid.Fid.Unique & 0xffffff));
	afs_PutCell(tcell, READ_LOCK);
	if (avc->f.fid.Fid.Vnode > 0xffff)
	    afs_fid_vnodeoverflow++;
	if (avc->f.fid.Fid.Unique > 0xffffff)
	    afs_fid_uniqueoverflow++;
    } else {
#if defined(AFS_SUN5_64BIT_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZPTR == 64))
	addr[1] = (long)AFS_XLATOR_MAGIC << 48;
#else /* defined(AFS_SGI61_ENV) && (_MIPS_SZPTR == 64) */
	addr[1] = AFS_XLATOR_MAGIC;
	SizeOfSmallFid = sizeof(addr);
#endif /* defined(AFS_SGI61_ENV) && (_MIPS_SZPTR == 64) */
	addr[0] = (long)avc;
#ifndef AFS_AIX41_ENV
	/* No post processing, so don't hold ref count. */
	AFS_FAST_HOLD(avc);
#endif
    }
#if	defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV)
    /* Use the fid pointer passed to us. */
    fidpp->fid_len = SizeOfSmallFid;

    if (afs_NFSRootOnly) {
	if (rootvp
#ifdef AFS_AIX41_ENV
	    || USE_SMALLFID(credp)
#endif
	    ) {
	    memcpy(fidpp->fid_data, (caddr_t) & Sfid, SizeOfSmallFid);
	} else {
	    memcpy(fidpp->fid_data, (caddr_t) addr, SizeOfSmallFid);
	}
    } else {
	memcpy(fidpp->fid_data, (caddr_t) & Sfid, SizeOfSmallFid);
    }
#else
    /* malloc a fid pointer ourselves. */
    *fidpp = (struct fid *)AFS_KALLOC(SizeOfSmallFid + 2);
    (*fidpp)->fid_len = SizeOfSmallFid;
    if (afs_NFSRootOnly) {
	if (rootvp) {
	    memcpy((*fidpp)->fid_data, (char *)&Sfid, SizeOfSmallFid);
	} else {
	    memcpy((*fidpp)->fid_data, (char *)addr, SizeOfSmallFid);
	}
    } else {
	memcpy((*fidpp)->fid_data, (char *)&Sfid, SizeOfSmallFid);
    }
#endif
    return (0);
}


#endif /* !AFS_LINUX20_ENV */
