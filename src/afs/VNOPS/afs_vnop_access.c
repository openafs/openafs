/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_vnop_access.c - access vop ccess mode bit support for vnode operations.
 *
 * Implements:
 * afs_GetAccessBits
 * afs_AccessOK
 * afs_access
 *
 * Local:
 * fileModeMap (table)
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"

#ifndef ANONYMOUSID
#define ANONYMOUSID     32766	/* make sure this is same as in ptserver.h */
#endif




/* access bits to turn off for various owner Unix mode values */
static char fileModeMap[8] = {
    PRSFS_READ | PRSFS_WRITE,
    PRSFS_READ | PRSFS_WRITE,
    PRSFS_READ,
    PRSFS_READ,
    PRSFS_WRITE,
    PRSFS_WRITE,
    0,
    0
};

/* avc must be held.  Returns bit map of mode bits.  Ignores file mode bits */
afs_int32
afs_GetAccessBits(struct vcache *avc, afs_int32 arights,
		  struct vrequest *areq)
{
    AFS_STATCNT(afs_GetAccessBits);
    /* see if anyuser has the required access bits */
    if ((arights & avc->f.anyAccess) == arights) {
	return arights;
    }

    /* look in per-pag cache */
    if (avc->Access) {		/* not beautiful, but Sun's cc will tolerate it */
	struct axscache *ac;

	ac = afs_FindAxs(avc->Access, areq->uid);
	if (ac) {
	    return (arights & ac->axess);
	}
    }

    if (!(avc->f.states & CForeign)) {
	/* If there aren't any bits cached for this user (but the vnode
	 * _is_ cached, obviously), make sure this user has valid tokens
	 * before bothering with the RPC.  */
	struct unixuser *tu;
	tu = afs_FindUser(areq->uid, avc->f.fid.Cell, READ_LOCK);
	if (!tu) {
	    return (arights & avc->f.anyAccess);
	}
	if ((tu->vid == UNDEFVID) || !(tu->states & UHasTokens)
	    || (tu->states & UTokensBad)) {
	    afs_PutUser(tu, READ_LOCK);
	    return (arights & avc->f.anyAccess);
	} else {
	    afs_PutUser(tu, READ_LOCK);
	}
    }

    if (AFS_IS_DISCONNECTED && !AFS_IN_SYNC) {
	/* If we get this far, we have to ask the network. But we can't, so
	 * they're out of luck... */
	return 0;
    } else
    {				/* Ok, user has valid tokens, go ask the server. */
	struct AFSFetchStatus OutStatus;
	afs_int32 code;

	code = afs_FetchStatus(avc, &avc->f.fid, areq, &OutStatus);
	return (code ? 0 : OutStatus.CallerAccess & arights);
    }
}


/* the new access ok function.  AVC must be held but not locked. if avc is a
 * file, its parent need not be held, and should not be locked. */

int
afs_AccessOK(struct vcache *avc, afs_int32 arights, struct vrequest *areq,
	     afs_int32 check_mode_bits)
{
    struct vcache *tvc;
    struct VenusFid dirFid;
    afs_int32 mask;
    afs_int32 dirBits;
    afs_int32 fileBits;

    AFS_STATCNT(afs_AccessOK);

    if ((vType(avc) == VDIR) || (avc->f.states & CForeign)) {
	/* rights are just those from acl */
	if (afs_InReadDir(avc)) {
	    /* if we are already in readdir, then they may have read and
	     * lookup, and nothing else, and nevermind the real ACL.
	     * Otherwise we might end up with problems trying to call
	     * FetchStatus on the vnode readdir is working on, and that
	     * would be a real mess.
	     */
	    dirBits = PRSFS_LOOKUP | PRSFS_READ;
	    return (arights == (dirBits & arights));
	}
	return (arights == afs_GetAccessBits(avc, arights, areq));
    } else {
	/* some rights come from dir and some from file.  Specifically, you 
	 * have "a" rights to a file if you are its owner, which comes
	 * back as "a" rights to the file. You have other rights just
	 * from dir, but all are restricted by the file mode bit. Now,
	 * if you have I and A rights to a file, we throw in R and W
	 * rights for free. These rights will then be restricted by
	 * the access mask. */
	dirBits = 0;
	if (avc->f.parent.vnode) {
	    dirFid.Cell = avc->f.fid.Cell;
	    dirFid.Fid.Volume = avc->f.fid.Fid.Volume;
	    dirFid.Fid.Vnode = avc->f.parent.vnode;
	    dirFid.Fid.Unique = avc->f.parent.unique;
	    /* Avoid this GetVCache call */
	    tvc = afs_GetVCache(&dirFid, areq, NULL, NULL);
	    if (tvc) {
		dirBits = afs_GetAccessBits(tvc, arights, areq);
		afs_PutVCache(tvc);
	    }
	} else
	    dirBits = 0xffffffff;	/* assume OK; this is a race condition */
	if (arights & PRSFS_ADMINISTER)
	    fileBits = afs_GetAccessBits(avc, arights, areq);
	else
	    fileBits = 0;	/* don't make call if results don't matter */

	/* compute basic rights in fileBits, taking A from file bits */
	fileBits =
	    (fileBits & PRSFS_ADMINISTER) | (dirBits & ~PRSFS_ADMINISTER);

	/* for files, throw in R and W if have I and A (owner).  This makes
	 * insert-only dirs work properly */
	if (vType(avc) != VDIR
	    && (fileBits & (PRSFS_ADMINISTER | PRSFS_INSERT)) ==
	    (PRSFS_ADMINISTER | PRSFS_INSERT))
	    fileBits |= (PRSFS_READ | PRSFS_WRITE);

	if (check_mode_bits & CHECK_MODE_BITS) {
	    /* owner mode bits are further restrictions on the access mode
	     * The mode bits are mapped to protection bits through the
	     * fileModeMap. If CMB_ALLOW_EXEC_AS_READ is set, it's from the
	     * NFS translator and we don't know if it's a read or execute
	     * on the NFS client, but both need to read the data.
	     */
	    mask = (avc->f.m.Mode & 0700) >> 6;	/* file restrictions to use */
	    fileBits &= ~fileModeMap[mask];
	    if (check_mode_bits & CMB_ALLOW_EXEC_AS_READ) {
		if (avc->f.m.Mode & 0100)
		    fileBits |= PRSFS_READ;
	    }
	}
	return ((fileBits & arights) == arights);	/* true if all rights bits are on */
    }
}


#if defined(AFS_SUN5_ENV) || (defined(AFS_SGI_ENV) && !defined(AFS_SGI65_ENV))
int
afs_access(OSI_VC_DECL(avc), afs_int32 amode, int flags,
	   afs_ucred_t *acred)
#else
int
afs_access(OSI_VC_DECL(avc), afs_int32 amode,
	   afs_ucred_t *acred)
#endif
{
    afs_int32 code;
    struct vrequest *treq = NULL;
    struct afs_fakestat_state fakestate;
    OSI_VC_CONVERT(avc);

    AFS_STATCNT(afs_access);
    afs_Trace3(afs_iclSetp, CM_TRACE_ACCESS, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, amode, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->f.m.Length));

    afs_InitFakeStat(&fakestate);
    if ((code = afs_CreateReq(&treq, acred))) {
	return code;
    }

    AFS_DISCON_LOCK();

    if (afs_fakestat_enable && avc->mvstat == 1) {
	code = afs_TryEvalFakeStat(&avc, &fakestate, treq);
        if (code == 0 && avc->mvstat == 1) {
	    afs_PutFakeStat(&fakestate);
	    AFS_DISCON_UNLOCK();
	    afs_DestroyReq(treq);
	    return 0;
        }
    } else {
	code = afs_EvalFakeStat(&avc, &fakestate, treq);
    }

    if (code) {
	afs_PutFakeStat(&fakestate);
	AFS_DISCON_UNLOCK();
	afs_DestroyReq(treq);
	return code;
    }

    if (vType(avc) != VDIR || !afs_InReadDir(avc)) {
	code = afs_VerifyVCache(avc, treq);
	if (code) {
	    afs_PutFakeStat(&fakestate);
	    AFS_DISCON_UNLOCK();
	    code = afs_CheckCode(code, treq, 16);
	    afs_DestroyReq(treq);
	    return code;
	}
    }

    /* if we're looking for write access and we have a read-only file system, report it */
    if ((amode & VWRITE) && (avc->f.states & CRO)) {
	afs_PutFakeStat(&fakestate);
	AFS_DISCON_UNLOCK();
	afs_DestroyReq(treq);
	return EROFS;
    }
    
    /* If we're looking for write access, and we're disconnected without logging, forget it */
    if ((amode & VWRITE) && (AFS_IS_DISCONNECTED && !AFS_IS_DISCON_RW)) {
        afs_PutFakeStat(&fakestate);
	AFS_DISCON_UNLOCK();
	/* printf("Network is down in afs_vnop_access\n"); */
	afs_DestroyReq(treq);
        return ENETDOWN;
    }
    
    code = 1;			/* Default from here on in is access ok. */
    if (avc->f.states & CForeign) {
	/* In the dfs xlator the EXEC bit is mapped to LOOKUP */
	if (amode & VEXEC)
	    code = afs_AccessOK(avc, PRSFS_LOOKUP, treq, CHECK_MODE_BITS);
	if (code && (amode & VWRITE)) {
	    code = afs_AccessOK(avc, PRSFS_WRITE, treq, CHECK_MODE_BITS);
	    if (code && (vType(avc) == VDIR)) {
		if (code)
		    code =
			afs_AccessOK(avc, PRSFS_INSERT, treq,
				     CHECK_MODE_BITS);
		if (!code)
		    code =
			afs_AccessOK(avc, PRSFS_DELETE, treq,
				     CHECK_MODE_BITS);
	    }
	}
	if (code && (amode & VREAD))
	    code = afs_AccessOK(avc, PRSFS_READ, treq, CHECK_MODE_BITS);
    } else {
	if (vType(avc) == VDIR) {
	    if (amode & VEXEC)
		code =
		    afs_AccessOK(avc, PRSFS_LOOKUP, treq, CHECK_MODE_BITS);
	    if (code && (amode & VWRITE)) {
		code =
		    afs_AccessOK(avc, PRSFS_INSERT, treq, CHECK_MODE_BITS);
		if (!code)
		    code =
			afs_AccessOK(avc, PRSFS_DELETE, treq,
				     CHECK_MODE_BITS);
	    }
	    if (code && (amode & VREAD))
		code =
		    afs_AccessOK(avc, PRSFS_LOOKUP, treq, CHECK_MODE_BITS);
	} else {
	    if (amode & VEXEC) {
		code = afs_AccessOK(avc, PRSFS_READ, treq, CHECK_MODE_BITS);
		if (code) {
			if ((avc->f.m.Mode & 0100) == 0)
			    code = 0;
		} else if (avc->f.m.Mode & 0100)
		    code = 1;
	    }
	    if (code && (amode & VWRITE)) {
		code = afs_AccessOK(avc, PRSFS_WRITE, treq, CHECK_MODE_BITS);

		/* The above call fails when the NFS translator tries to copy
		 ** a file with r--r--r-- permissions into a directory which
		 ** has system:anyuser acl. This is because the destination file
		 ** file is first created with r--r--r-- permissions through an
		 ** unauthenticated connectin.  hence, the above afs_AccessOK
		 ** call returns failure. hence, we retry without any file 
		 ** mode bit checking */
		if (!code && AFS_NFSXLATORREQ(acred)
		    && avc->f.m.Owner == ANONYMOUSID)
		    code =
			afs_AccessOK(avc, PRSFS_WRITE, treq,
				     DONT_CHECK_MODE_BITS);
	    }
	    if (code && (amode & VREAD))
		code = afs_AccessOK(avc, PRSFS_READ, treq, CHECK_MODE_BITS);
	}
    }
    afs_PutFakeStat(&fakestate);

    AFS_DISCON_UNLOCK();
    
    if (code) {
	afs_DestroyReq(treq);
	return 0;		/* if access is ok */
    } else {
	code = afs_CheckCode(EACCES, treq, 17);	/* failure code */
	afs_DestroyReq(treq);
	return code;
    }
}

#if defined(UKERNEL)
/*
 * afs_getRights
 * This function is just an interface to afs_GetAccessBits
 */
int
afs_getRights(OSI_VC_DECL(avc), afs_int32 arights,
	      afs_ucred_t *acred)
{
    afs_int32 code;
    struct vrequest *treq = NULL;
    OSI_VC_CONVERT(avc);

    if ((code = afs_CreateReq(&treq, acred)))
	return code;

    code = afs_VerifyVCache(avc, treq);
    if (code) {
	code = afs_CheckCode(code, treq, 16);
	afs_DestroyReq(treq);
	return code;
    }

    code = afs_GetAccessBits(avc, arights, treq);
    afs_DestroyReq(treq);
    return code;
}
#endif /* defined(UKERNEL) */
