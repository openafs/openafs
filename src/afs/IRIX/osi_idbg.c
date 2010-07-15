/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implementation of Irix IDBG facility for AFS.
 */
#include <afsconfig.h>
#include "afs/param.h"


#ifdef	AFS_SGI62_ENV
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */


/*
 * debugging routine - invoked by calling kp vnode xx
 */
static void
printflags(unsigned int flags, char **strings)
{
    int mask = 1;

    while (flags != 0) {
	if (mask & flags) {
	    qprintf("%s ", *strings);
	    flags &= ~mask;
	}
	mask <<= 1;
	strings++;
    }
}

char *tab_vcache[] = {
    "CStatd",			/* 1 */
    "CBackup",			/* 2 */
    "CRO",			/* 4 */
    "CMValid",			/* 8 */
    "CCore",			/* 0x10 */
    "CDirty",			/* 0x20 */
    "CSafeStore",		/* 0x40 */
    "CMAPPED",			/* 0x80 */
    "CNSHARE",			/* 0x100 */
};

int
idbg_prafsnode(OSI_VC_DECL(avc))
{
    OSI_VC_CONVERT(avc);

    AFS_GLOCK();
    qprintf("   Len %d DV %d Date %d Own %d Grp %d Mode 0%o Lnk %d\n",
	    avc->f.m.Length, avc->f.m.DataVersion, avc->f.m.Date, avc->f.m.Owner,
	    avc->f.m.Group, avc->f.m.Mode, avc->f.m.LinkCount);
    qprintf("   flushDV %d mapDV %d truncpos 0x%x cb 0x%x cbE 0x%x\n",
	    avc->flushDV, avc->mapDV, avc->f.truncPos, avc->callback,
	    avc->cbExpires);
    qprintf("   opens %d ex/wr %d flckcnt %d state 0x%x ", avc->opens,
	    avc->execsOrWriters, avc->flockCount, avc->f.states);
    printflags(avc->f.states, tab_vcache);
    qprintf("\n");
#ifdef AFS_SGI64_ENV
    qprintf("   mapcnt %llu, mvstat %d anyAcc 0x%x Access 0x%x\n",
	    avc->mapcnt, avc->mvstat, avc->f.anyAccess, avc->Access);
    qprintf("   mvid 0x%x &lock 0x%x cred 0x%x\n", avc->mvid, &avc->lock,
	    avc->cred);
    qprintf("   rwlock 0x%x (%d) id %llu trips %d\n", &avc->vc_rwlock,
	    valusema(&avc->vc_rwlock), avc->vc_rwlockid, avc->vc_locktrips);
#else
    qprintf("   mapcnt %d mvstat %d anyAcc 0x%x Access 0x%x\n", avc->mapcnt,
	    avc->mvstat, avc->f.anyAccess, avc->Access);
    qprintf("   mvid 0x%x &lock 0x%x cred 0x%x\n", avc->mvid, &avc->lock,
	    avc->cred);
    qprintf("   rwlock 0x%x (%d) id %d trips %d\n", &avc->vc_rwlock,
	    valusema(&avc->vc_rwlock), avc->vc_rwlockid, avc->vc_locktrips);
#endif
    AFS_GUNLOCK();
    return 0;
}

extern struct afs_q VLRU;	/*vcache LRU */
static char *tab_vtypes[] = {
    "VNON",
    "VREG",
    "VDIR",
    "VBLK",
    "VCHR",
    "VLNK",
    "VFIFO",
    "VXNAM",
    "VBAD",
    "VSOCK",
    0
};

int
idbg_afsvfslist()
{
    struct vcache *tvc;
    struct afs_q *tq;
    struct afs_q *uq;
    afs_int32 nodeid;		/* what ls prints as 'inode' */

    AFS_GLOCK();
    for (tq = VLRU.prev; tq != &VLRU; tq = uq) {
	tvc = QTOV(tq);
	uq = QPrev(tq);
	nodeid = tvc->f.fid.Fid.Vnode + (tvc->f.fid.Fid.Volume << 16);
	nodeid &= 0x7fffffff;
	qprintf("avp 0x%x type %s cnt %d pg %d map %d nodeid %d(0x%x)\n", tvc,
		tab_vtypes[((vnode_t *) tvc)->v_type],
		((vnode_t *) tvc)->v_count,
		(int)VN_GET_PGCNT((vnode_t *) tvc), (int)tvc->mapcnt, nodeid,
		nodeid);
    }
    AFS_GUNLOCK();
    return 0;
}

static char *tab_userstates[] = {
    "UHasTokens",
    "UTokensBad",
    "UPrimary",
    "UNeedsReset",
    "UPAGcounted",
    "UNK",
};

static void
idbg_pruser(struct unixuser *tu)
{
    qprintf("@0x%x nxt 0x%x uid %d (0x%x) cell 0x%x vid 0x%x ref %d\n", tu,
	    tu->next, tu->uid, tu->uid, tu->cell, tu->vid, tu->refCount);
    qprintf("time %d stLen %d stp 0x%x exp 0x%x ", tu->tokenTime, tu->stLen,
	    tu->stp, tu->exporter);
    printflags(tu->states, tab_userstates);
    qprintf("\n");
    qprintf("ClearToken: handle 0x%x ViceID 0x%x Btime %d Etime %d\n",
	    tu->ct.AuthHandle, tu->ct.ViceId, tu->ct.BeginTimestamp,
	    tu->ct.EndTimestamp);
}

extern struct unixuser *afs_users[NUSERS];
int
idbg_afsuser(void *x)
{
    struct unixuser *tu;
    int i;
    AFS_GLOCK();

    if (x == (void *)-1L) {
	for (i = 0; i < NUSERS; i++)
	    for (tu = afs_users[i]; tu; tu = tu->next)
		idbg_pruser(tu);
    } else
	idbg_pruser((struct unixuser *)x);

    AFS_GUNLOCK();
    return 0;
}

#endif /* AFS_SGI62_ENV */
