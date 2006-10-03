/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __CM_DCACHE_ENV__
#define __CM_DCACHE_ENV__ 1

/* bulk I/O descriptor */
typedef struct cm_bulkIO {
    struct cm_scache *scp;		/* typically unheld vnode ptr */
    osi_hyper_t offset;		        /* offset of buffers */
    long length;			/* # of bytes to be transferred */
    int reserved;			/* did we reserve multiple buffers? */
        
    /* all of these buffers are held */
    osi_queueData_t *bufListp;	/* list of buffers involved in I/O */
    osi_queueData_t *bufListEndp;	/* list of buffers involved in I/O */
} cm_bulkIO_t;

extern long cm_StoreMini(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern int cm_InitDCache(int newFile, long chunkSize, afs_uint64 nbuffers);

extern int cm_HaveBuffer(struct cm_scache *, struct cm_buf *, int haveBufLocked);

extern long cm_GetBuffer(struct cm_scache *, struct cm_buf *, int *,
	struct cm_user *, struct cm_req *);

extern long cm_CheckFetchRange(cm_scache_t *scp, osi_hyper_t *startBasep,
	long length, cm_user_t *up, cm_req_t *reqp, osi_hyper_t *realBasep);

extern long cm_SetupFetchBIOD(cm_scache_t *scp, osi_hyper_t *offsetp,
	cm_bulkIO_t *biop, cm_user_t *up, cm_req_t *reqp);

extern void cm_ReleaseBIOD(cm_bulkIO_t *biop, int isStore);

extern long cm_SetupStoreBIOD(cm_scache_t *scp, osi_hyper_t *inOffsetp,
	long inSize, cm_bulkIO_t *biop, cm_user_t *userp, cm_req_t *reqp);

extern void cm_BkgPrefetch(cm_scache_t *scp, afs_uint32 p1, afs_uint32 p2, afs_uint32 p3, afs_uint32 p4,
	struct cm_user *userp);

extern void cm_BkgStore(cm_scache_t *scp, afs_uint32 p1, afs_uint32 p2, afs_uint32 p3, afs_uint32 p4,
	struct cm_user *userp);

extern void cm_ConsiderPrefetch(cm_scache_t *scp, osi_hyper_t *offsetp,
	cm_user_t *userp, cm_req_t *reqp);

extern long cm_ValidateDCache(void);

extern long cm_ShutdownDCache(void);

#endif /*  __CM_DCACHE_ENV__ */
