/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
#ifndef __CM_DAEMON_H_ENV_
#define __CM_DAEMON_H_ENV_ 1

/* externs */
extern long cm_daemonCheckInterval;

extern osi_rwlock_t cm_daemonLock;

void cm_InitDaemon(int nDaemons);

typedef void (cm_bkgProc_t)(cm_scache_t *scp, long p1, long p2, long p3,
	long p4, struct cm_user *up);

typedef struct cm_bkgRequest {
	osi_queue_t q;
	cm_bkgProc_t *procp;
        cm_scache_t *scp;
        long p1;
        long p2;
        long p3;
        long p4;
        struct cm_user *userp;
} cm_bkgRequest_t;

extern void cm_QueueBKGRequest(cm_scache_t *scp, cm_bkgProc_t *procp, long p1,
	long p2, long p3, long p4, cm_user_t *userp);

#endif /*  __CM_DAEMON_H_ENV_ */
