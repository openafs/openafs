/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _CM_CALLBACK_H_ENV__
#define _CM_CALLBACK_H_ENV__ 1

#include <osi.h>

typedef struct cm_callbackRequest {
    long callbackCount;		/* callback count at start of the request */
    time_t startTime;	/* time when we started the call */
    struct cm_server *serverp;	/* server we really got the callback from */
} cm_callbackRequest_t;

#include "cm_scache.h"

typedef struct cm_racingRevokes {
    osi_queue_t q;		/* queue for forward/backward searches */
    cm_fid_t fid;		/* fid revoked */
    long callbackCount;		/* which callback this is */
    long flags;
} cm_racingRevokes_t;

/* flags for cm_racingRevokes_t flags field */
#define CM_RACINGFLAG_CANCELALL		1	/* cancels all racing callback grants */
#define CM_RACINGFLAG_CANCELVOL		2	/* cancels all this volume */
/* and one representing the union of all cancel descrs */
#define CM_RACINGFLAG_ALL		(CM_RACINGFLAG_CANCELALL | CM_RACINGFLAG_CANCELVOL)

/* flag for calls to functions in this package */
#define CM_CALLBACK_MAINTAINCOUNT	1	/* don't decrement count of
						 * callback-granting calls.
                                                 */

/* Combinations of change notification filters to make sure callback loss
 * gets noticed
 */
#define FILE_NOTIFY_GENERIC_DIRECTORY_FILTER \
	(FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME)
#define FILE_NOTIFY_GENERIC_FILE_FILTER \
	(FILE_NOTIFY_CHANGE_ATTRIBUTES \
	 | FILE_NOTIFY_CHANGE_SIZE \
	 | FILE_NOTIFY_CHANGE_LAST_WRITE \
	 | FILE_NOTIFY_CHANGE_LAST_ACCESS \
	 | FILE_NOTIFY_CHANGE_CREATION)

extern void cm_InitCallback(void);

extern int cm_HaveCallback(struct cm_scache *);

extern void cm_StartCallbackGrantingCall(struct cm_scache *, cm_callbackRequest_t *);

extern void cm_EndCallbackGrantingCall(struct cm_scache *, cm_callbackRequest_t *,
	struct AFSCallBack *, long);

extern long cm_GetCallback(struct cm_scache *, struct cm_user *,
	struct cm_req * reqp, long flags);

extern void cm_CheckCBExpiration(void);

extern osi_rwlock_t cm_callbackLock;

extern void cm_CallbackNotifyChange(cm_scache_t *scp);

extern void cm_GiveUpAllCallbacks(cm_server_t *tsp);

extern void cm_GiveUpAllCallbacksAllServers(void);

#endif /*  _CM_CALLBACK_H_ENV__ */
