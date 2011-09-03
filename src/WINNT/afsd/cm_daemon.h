/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_CM_DAEMON_H
#define OPENAFS_WINNT_AFSD_CM_DAEMON_H 1

/* externs */
extern long cm_daemonCheckDownInterval;
extern long cm_daemonCheckUpInterval;
extern long cm_daemonCheckVolInterval;
extern long cm_daemonCheckCBInterval;
extern long cm_daemonCheckLockInterval;
extern long cm_daemonTokenCheckInterval;

extern osi_rwlock_t cm_daemonLock;

void cm_DaemonShutdown(void);

void cm_InitDaemon(int nDaemons);

typedef afs_int32 (cm_bkgProc_t)(cm_scache_t *scp, afs_uint32 p1, afs_uint32 p2, afs_uint32 p3,
	afs_uint32 p4, struct cm_user *up, cm_req_t *reqp);

typedef struct cm_bkgRequest {
	osi_queue_t q;
	cm_bkgProc_t *procp;
        cm_scache_t *scp;
        afs_uint32 p1;
        afs_uint32 p2;
        afs_uint32 p3;
        afs_uint32 p4;
        cm_user_t *userp;
        cm_req_t req;
} cm_bkgRequest_t;

extern void cm_QueueBKGRequest(cm_scache_t *scp, cm_bkgProc_t *procp, afs_uint32 p1,
	afs_uint32 p2, afs_uint32 p3, afs_uint32 p4, cm_user_t *userp, cm_req_t *reqp);

#define CM_MAX_DAEMONS 64

#endif /*  OPENAFS_WINNT_AFSD_CM_DAEMON_H */
