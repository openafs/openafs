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

#include "afs/sysincludes.h"	/*Standard vendor system headers */
#include "afsincludes.h"	/*AFS-based standard headers */

#include "osi_compat.h"

static void
TryEvictDirDentries(struct inode *inode)
{
    struct dentry *dentry;
#if defined(D_ALIAS_IS_HLIST) && !defined(HLIST_ITERATOR_NO_NODE)
    struct hlist_node *p;
#endif

    afs_d_alias_lock(inode);

restart:
#if defined(D_ALIAS_IS_HLIST)
# if defined(HLIST_ITERATOR_NO_NODE)
    hlist_for_each_entry(dentry, &inode->i_dentry, d_alias) {
# else
    hlist_for_each_entry(dentry, p, &inode->i_dentry, d_alias) {
# endif
#else
    list_for_each_entry(dentry, &inode->i_dentry, d_alias) {
#endif
	spin_lock(&dentry->d_lock);
	if (d_unhashed(dentry)) {
	    spin_unlock(&dentry->d_lock);
	    continue;
	}
	spin_unlock(&dentry->d_lock);
	afs_linux_dget(dentry);

	afs_d_alias_unlock(inode);

	/*
	 * Once we have dropped the d_alias lock (above), it is no longer safe
	 * to 'continue' our iteration over d_alias because the list may change
	 * out from under us.  Therefore, we must either leave the loop, or
	 * restart from the beginning.  To avoid looping forever, we must only
	 * restart if we know we've d_drop'd an alias.  In all other cases we
	 * must leave the loop.
	 */

	/*
	 * For a long time we used d_invalidate() for this purpose, but
	 * using shrink_dcache_parent() and checking the refcount ourselves is
	 * better, for two reasons: it avoids causing ENOENT issues for the
	 * CWD in linux versions since 3.11, and it avoids dropping Linux
	 * submounts.
	 *
	 * For non-fakestat, AFS mountpoints look like directories and end up here.
	 */

	shrink_dcache_parent(dentry);
	spin_lock(&dentry->d_lock);
	if (afs_dentry_count(dentry) > 1)	/* still has references */ {
	    if (dentry->d_inode != NULL) /* is not a negative dentry */ {
		spin_unlock(&dentry->d_lock);
		dput(dentry);
		goto inuse;
	    }
	}
	/*
	 * This is either a negative dentry, or a dentry with no references.
	 * Either way, it is okay to unhash it now.
	 * Do so under the d_lock (that is, via __d_drop() instead of d_drop())
	 * to avoid a race with another process picking up a reference.
	 */
	__d_drop(dentry);
	spin_unlock(&dentry->d_lock);

	dput(dentry);
	afs_d_alias_lock(inode);
	goto restart;
    }
    afs_d_alias_unlock(inode);

inuse:
    return;
}


int
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep)
{
    int code;

    /* First, see if we can evict the inode from the dcache */
    if (defersleep && avc != afs_globalVp && VREFCOUNT(avc) > 1
	&& avc->opens == 0) {
	struct inode *ip = AFSTOV(avc);

	*slept = 1;
	AFS_FAST_HOLD(avc);
	ReleaseWriteLock(&afs_xvcache);
	AFS_GUNLOCK();

	if (S_ISDIR(ip->i_mode))
	    TryEvictDirDentries(ip);
	else
	    d_prune_aliases(ip);

	AFS_GLOCK();
	ObtainWriteLock(&afs_xvcache, 733);
	AFS_FAST_RELE(avc);
    }

    /* See if we can evict it from the VLRUQ */
    if (VREFCOUNT_GT(avc, 0) && !VREFCOUNT_GT(avc, 1) && avc->opens == 0
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
    ip->u.generic_ip = tvc;
    tvc->v = ip;
#endif

    INIT_LIST_HEAD(&tvc->pagewriters);
    spin_lock_init(&tvc->pagewriter_lock);

    return tvc;
}

void
osi_PrePopulateVCache(struct vcache *avc)
{
    avc->uncred = 0;
    memset(&(avc->f), 0, sizeof(struct fvcache));
    avc->cred = NULL;
}

void
osi_AttachVnode(struct vcache *avc, int seq)
{
    /* Nada */
}

void
osi_PostPopulateVCache(struct vcache *avc)
{
    vSetType(avc, VREG);
}

/**
 * osi_ResetRootVCache - Reset the root vcache
 * Reset the dentry associated with the afs root.
 * Called from afs_CheckRootVolume when we notice that
 * the root volume ID has changed.
 *
 * @volid: volume ID for the afs root
 */
void
osi_ResetRootVCache(afs_uint32 volid)
{
    struct vrequest *treq = NULL;
    struct vattr vattr;
    cred_t *credp;
    struct dentry *dp;
    struct vcache *vcp;
    struct inode *root = AFSTOV(afs_globalVp);

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

    dp = d_find_alias(root);

    afs_d_alias_lock(AFSTOV(vcp));

    spin_lock(&dp->d_lock);
#if defined(D_ALIAS_IS_HLIST)
    hlist_del_init(&dp->d_alias);
    hlist_add_head(&dp->d_alias, &(AFSTOV(vcp)->i_dentry));
#else
    list_del_init(&dp->d_alias);
    list_add(&dp->d_alias, &(AFSTOV(vcp)->i_dentry));
#endif
    dp->d_inode = AFSTOV(vcp);
    spin_unlock(&dp->d_lock);

    afs_d_alias_unlock(AFSTOV(vcp));

    dput(dp);

    AFS_RELE(root);
    afs_globalVp = vcp;
out:
    crfree(credp);
    afs_DestroyReq(treq);
}
