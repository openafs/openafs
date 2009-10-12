/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux support routines.
 *
 */
#include <afsconfig.h>
#include "afs/param.h"


#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"
#if defined(AFS_LINUX24_ENV)
#include "h/smp_lock.h"
#endif

int afs_osicred_initialized = 0;
afs_ucred_t afs_osi_cred;

void
afs_osi_SetTime(osi_timeval_t * tvp)
{
#if defined(AFS_LINUX24_ENV)
    struct timeval tv;
    tv.tv_sec = tvp->tv_sec;
    tv.tv_usec = tvp->tv_usec;

    AFS_STATCNT(osi_SetTime);

    do_settimeofday(&tv);
#else
    extern int (*sys_settimeofdayp) (struct timeval * tv,
				     struct timezone * tz);

    KERNEL_SPACE_DECL;

    AFS_STATCNT(osi_SetTime);

    TO_USER_SPACE();
    if (sys_settimeofdayp)
	(void)(*sys_settimeofdayp) (tvp, NULL);
    TO_KERNEL_SPACE();
#endif
}

void
osi_linux_mask(void)
{
    SIG_LOCK(current);
    sigfillset(&current->blocked);
    RECALC_SIGPENDING(current);
    SIG_UNLOCK(current);
}

#if defined(AFS_LINUX24_ENV)
/* LOOKUP_POSITIVE is becoming the default */
#ifndef LOOKUP_POSITIVE
#define LOOKUP_POSITIVE 0
#endif
/* Lookup name and return vnode for same. */
int
osi_lookupname_internal(char *aname, int followlink, struct vfsmount **mnt,
			struct dentry **dpp)
{
    int code;
    struct nameidata nd;
    int flags = LOOKUP_POSITIVE;
    code = ENOENT;

    if (followlink)
       flags |= LOOKUP_FOLLOW;
    if (path_init(aname, flags, &nd))
        code = path_walk(aname, &nd);

    if (!code) {
#if defined(STRUCT_NAMEIDATA_HAS_PATH)
	*dpp = dget(nd.path.dentry);
        if (mnt)
	    *mnt = mntget(nd.path.mnt);
	path_put(&nd.path);
#else
	*dpp = dget(nd.dentry);
        if (mnt)
           *mnt = mntget(nd.mnt);
	path_release(&nd);
#endif
    }
    return code;
}

int
osi_lookupname(char *aname, uio_seg_t seg, int followlink, 
	       struct dentry **dpp)
{
    int code;
    char *tname;
    code = ENOENT;
    if (seg == AFS_UIOUSER) {
        tname = getname(aname);
        if (IS_ERR(tname)) 
            return PTR_ERR(tname);
    } else {
        tname = aname;
    }
    code = osi_lookupname_internal(tname, followlink, NULL, dpp);   
    if (seg == AFS_UIOUSER) {
        putname(tname);
    }
    return code;
}
#else
int
osi_lookupname(char *aname, uio_seg_t seg, int followlink, struct dentry **dpp)
{
    struct dentry *dp = NULL;
    int code;

    code = ENOENT;
    if (seg == AFS_UIOUSER) {
	dp = followlink ? namei(aname) : lnamei(aname);
    } else {
	dp = lookup_dentry(aname, NULL, followlink ? 1 : 0);
    }

    if (dp && !IS_ERR(dp)) {
	if (dp->d_inode) {
	    *dpp = dp;
	    code = 0;
	} else
	    dput(dp);
    }

    return code;
}
#endif
