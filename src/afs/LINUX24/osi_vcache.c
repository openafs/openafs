/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

#include "afs/sysincludes.h"    /*Standard vendor system headers */
#include "afsincludes.h"        /*AFS-based standard headers */

#if defined(AFS_LINUX24_ENV)
# define afs_linux_lock_dcache() spin_lock(&dcache_lock)
# define afs_linux_unlock_dcache() spin_unlock(&dcache_lock)
#else
# define afs_linux_lock_dcache()
# define afs_linux_unlock_dcache()
#endif

int
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep) {
    int code;
    struct dentry *dentry;
    struct list_head *cur, *head;

    /* First, see if we can evict the inode from the dcache */
    if (defersleep && avc != afs_globalVp && VREFCOUNT(avc) > 1 && avc->opens == 0) {
	*slept = 1;
	ReleaseWriteLock(&afs_xvcache);
        AFS_GUNLOCK();
	afs_linux_lock_dcache();
	head = &(AFSTOV(avc))->i_dentry;

restart:
        cur = head;
	while ((cur = cur->next) != head) {
	    dentry = list_entry(cur, struct dentry, d_alias);

	    if (d_unhashed(dentry))
		continue;

	    dget_locked(dentry);

	    afs_linux_unlock_dcache();

	    if (d_invalidate(dentry) == -EBUSY) {
		dput(dentry);
		/* perhaps lock and try to continue? (use cur as head?) */
		goto inuse;
	    }
	    dput(dentry);
	    afs_linux_lock_dcache();
	    goto restart;
	}
	afs_linux_unlock_dcache();
inuse:
	AFS_GLOCK();
        ObtainWriteLock(&afs_xvcache, 733);
    }

    /* See if we can evict it from the VLRUQ */
    if (VREFCOUNT_GT(avc,0) && !VREFCOUNT_GT(avc,1) && avc->opens == 0
	&& (avc->f.states & CUnlinkedDel) == 0) {
	int didsleep = *slept;

	code = afs_FlushVCache(avc, slept);
        /* flushvcache wipes slept; restore slept if we did before */
        if (didsleep)
            *slept = didsleep;

	if (code == 0)
	   return 1;
    }

    return 0;
}

struct vcache *
osi_NewVnode(void)
{
    struct inode *ip;
    struct vcache *tvc;

    AFS_GUNLOCK();
    ip = new_inode(afs_globalVFS);
    if (!ip)
	osi_Panic("afs_NewVCache: no more inodes");
    AFS_GLOCK();
#if defined(STRUCT_SUPER_OPERATIONS_HAS_ALLOC_INODE)
    tvc = VTOAFS(ip);
#else
    tvc = afs_osi_Alloc(sizeof(struct vcache));
    osi_Assert(tvc != NULL);
    ip->u.generic_ip = tvc;
    tvc->v = ip;
#endif

    return tvc;
}

/* XXX - This should probably be inline */
void
osi_PrePopulateVCache(struct vcache *avc) {
    avc->uncred = 0;
    memset(&(avc->f), 0, sizeof(struct fvcache));
}

/* XXX - This should become inline, or a #define */
void
osi_AttachVnode(struct vcache *avc, int seq) { }

/* XXX - Again, inline or a #define */
void
osi_PostPopulateVCache(struct vcache *avc) {
    vSetType(avc, VREG);
}

void
osi_ResetRootVCache(afs_uint32 volid)
{
    struct vrequest *treq = NULL;
    struct vattr vattr;
    cred_t *credp;
    struct dentry *dp;
    struct vcache *vcp;

    afs_rootFid.Fid.Volume = volid;
    afs_rootFid.Fid.Vnode = 1;
    afs_rootFid.Fid.Unique = 1;

    credp = crref();
    if (afs_CreateReq(&treq, credp))
	goto out;
    vcp = afs_GetVCache(&afs_rootFid, treq, NULL, NULL);
    if (!vcp)
	goto out;
    afs_getattr(vcp, &vattr, credp);
    afs_fill_inode(AFSTOV(vcp), &vattr);

    dp = d_find_alias(AFSTOV(afs_globalVp));
    spin_lock(&dcache_lock);
    list_del_init(&dp->d_alias);
    list_add(&dp->d_alias, &(AFSTOV(vcp)->i_dentry));
    dp->d_inode = AFSTOV(vcp);
    spin_unlock(&dcache_lock);
    dput(dp);

    AFS_FAST_RELE(afs_globalVp);
    afs_globalVp = vcp;
out:
    crfree(credp);
    afs_DestroyReq(treq);
}
