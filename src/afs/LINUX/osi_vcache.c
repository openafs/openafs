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

int
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep) {
    int code;

    struct dentry *dentry;
    struct inode *inode = AFSTOV(avc);
#if defined(D_ALIAS_IS_HLIST)
    struct hlist_node *cur, *head, *list_end;
#else
    struct list_head *cur, *head, *list_end;
#endif

    /* First, see if we can evict the inode from the dcache */
    if (defersleep && avc != afs_globalVp && VREFCOUNT(avc) > 1 && avc->opens == 0) {
	*slept = 1;
	ReleaseWriteLock(&afs_xvcache);
        AFS_GUNLOCK();

#if defined(HAVE_DCACHE_LOCK)
        spin_lock(&dcache_lock);
	head = &inode->i_dentry;

restart:
        cur = head;
	while ((cur = cur->next) != head) {
	    dentry = list_entry(cur, struct dentry, d_alias);

	    if (d_unhashed(dentry))
		continue;
	    dget_locked(dentry);

	    spin_unlock(&dcache_lock);
	    if (d_invalidate(dentry) == -EBUSY) {
		dput(dentry);
		/* perhaps lock and try to continue? (use cur as head?) */
		goto inuse;
	    }
	    dput(dentry);
	    spin_lock(&dcache_lock);
	    goto restart;
	}
	spin_unlock(&dcache_lock);
#else /* HAVE_DCACHE_LOCK */
	spin_lock(&inode->i_lock);
#if defined(D_ALIAS_IS_HLIST)
	head = inode->i_dentry.first;
	list_end = NULL;
#else
	head = &inode->i_dentry;
	list_end = head;
#endif

restart:
	cur = head;
	while ((cur = cur->next) != list_end) {
#if defined(D_ALIAS_IS_HLIST)
	    dentry = hlist_entry(cur, struct dentry, d_alias);
#else
	    dentry = list_entry(cur, struct dentry, d_alias);
#endif

	    spin_lock(&dentry->d_lock);
	    if (d_unhashed(dentry)) {
		spin_unlock(&dentry->d_lock);
		continue;
	    }
	    spin_unlock(&dentry->d_lock);
	    dget(dentry);

	    spin_unlock(&inode->i_lock);
	    if (d_invalidate(dentry) == -EBUSY) {
		dput(dentry);
		/* perhaps lock and try to continue? (use cur as head?) */
		goto inuse;
	    }
	    dput(dentry);
	    spin_lock(&inode->i_lock);
	    goto restart;
	}
	spin_unlock(&inode->i_lock);
#endif /* HAVE_DCACHE_LOCK */
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
    ip->u.generic_ip = tvc;
    tvc->v = ip;
#endif

    return tvc;
}

void
osi_PrePopulateVCache(struct vcache *avc) {
    avc->uncred = 0;
    memset(&(avc->f), 0, sizeof(struct fvcache));
    avc->cred = NULL;
}

void
osi_AttachVnode(struct vcache *avc, int seq) { /* Nada */ }

void
osi_PostPopulateVCache(struct vcache *avc) {
    vSetType(avc, VREG);
}

