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
     /* we can't control whether we sleep */
     if (!VREFCOUNT_GT(avc,0)
         && avc->opens == 0 && (avc->f.states & CUnlinkedDel) == 0) {
        code = afs_FlushVCache(avc, slept);
	if (code == 0)
	    return 1;
     }
     return 0;
}

extern char *makesname();

struct vcache *
osi_NewVnode(void) {
    struct vcache *avc;
    char name[METER_NAMSZ];

    avc = afs_osi_Alloc(sizeof(struct vcache));
    if (avc == NULL) {
	return NULL;
    }

    memset(avc, 0, sizeof(struct vcache));
    avc->v.v_number = ++afsvnumbers;
    avc->vc_rwlockid = OSI_NO_LOCKID;
    initnsema(&avc->vc_rwlock, 1,
	      makesname(name, "vrw", avc->v.v_number));
    return avc;
}

void
osi_PrePopulateVCache(struct vcache *avc) {
    avc->uncred = 0;
    memset(&(avc->f), 0, sizeof(struct fvcache));
}

void
osi_AttachVnode(struct vcache *avc, int seq) { }

void
osi_PostPopulateVCache(struct vcache *avc) {
    memset(&(avc->vc_bhv_desc), 0, sizeof(avc->vc_bhv_desc));
    bhv_desc_init(&(avc->vc_bhv_desc), avc, avc, &Afs_vnodeops);

    vn_bhv_head_init(&(avc->v.v_bh), "afsvp");
    vn_bhv_insert_initial(&(avc->v.v_bh), &(avc->vc_bhv_desc));
    avc->v.v_mreg = avc->v.v_mregb = (struct pregion *)avc;
# if defined(VNODE_TRACING)
    avc->v.v_trace = ktrace_alloc(VNODE_TRACE_SIZE, 0);
# endif
    init_bitlock(&avc->v.v_pcacheflag, VNODE_PCACHE_LOCKBIT, "afs_pcache",
		 avc->v.v_number);
    init_mutex(&avc->v.v_filocksem, MUTEX_DEFAULT, "afsvfl", (long)avc);
    init_mutex(&avc->v.v_buf_lock, MUTEX_DEFAULT, "afsvnbuf", (long)avc);

    vnode_pcache_init(&avc->v);

#if defined(DEBUG) && defined(VNODE_INIT_BITLOCK)
    /* Above define is never true execpt in SGI test kernels. */
    init_bitlock(&avc->v.v_flag, VLOCK, "vnode", avc->v.v_number);
#endif

#ifdef INTR_KTHREADS
    AFS_VN_INIT_BUF_LOCK(&(avc->v));
#endif

    vSetVfsp(avc, afs_globalVFS);
    vSetType(avc, VREG);

    VN_SET_DPAGES(&(avc->v), NULL);
    osi_Assert((avc->v.v_flag & VINACT) == 0);
    avc->v.v_flag = 0;
    osi_Assert(VN_GET_PGCNT(&(avc->v)) == 0);
    osi_Assert(avc->mapcnt == 0 && avc->vc_locktrips == 0);
    osi_Assert(avc->vc_rwlockid == OSI_NO_LOCKID);
    osi_Assert(avc->v.v_filocks == NULL);
    osi_Assert(avc->cred == NULL);
    vnode_pcache_reinit(&avc->v);
    avc->v.v_rdev = NODEV;
    vn_initlist((struct vnlist *)&avc->v);
    avc->lastr = 0;
}

int
osi_vnhold(struct vcache *avc)
{
    VN_HOLD(AFSTOV(avc));
    return 0;
}
