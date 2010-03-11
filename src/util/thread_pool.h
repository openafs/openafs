/*
 * Copyright 2008-2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_UTIL_THREAD_POOL_H
#define AFS_UTIL_THREAD_POOL_H 1

#include "thread_pool_types.h"

/**
 * public interfaces for thread_pool.
 */

/* XXX move these into an et */
#define AFS_TP_ERROR              -1 /**< fatal error in thread_pool package */

extern int afs_tp_create(struct afs_thread_pool **,
			 struct afs_work_queue *);
extern int afs_tp_destroy(struct afs_thread_pool *);

extern int afs_tp_set_threads(struct afs_thread_pool *, afs_uint32 threads);
extern int afs_tp_set_entry(struct afs_thread_pool *,
			    afs_tp_worker_func_t *,
			    void * rock);

extern int afs_tp_start(struct afs_thread_pool *);
extern int afs_tp_shutdown(struct afs_thread_pool *,
			   int block);

extern int afs_tp_is_online(struct afs_thread_pool *);
extern int afs_tp_worker_continue(struct afs_thread_pool_worker *);

#endif /* AFS_UTIL_THREAD_POOL_H */
