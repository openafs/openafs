/*
 * Copyright 2008-2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_UTIL_THREAD_POOL_TYPES_H
#define AFS_UTIL_THREAD_POOL_TYPES_H 1

/**
 *  public type definitions for thread_pool.
 */

/* forward declare opaque types */
struct afs_thread_pool_worker;
struct afs_thread_pool;
struct afs_work_queue;

/**
 * thread_pool worker thread entry function.
 *
 * @param[in] pool    thread pool object pointer
 * @param[in] worker  worker thread object pointer
 * @param[in] queue   work queue object pointer
 * @param[in] rock    opaque pointer
 *
 * @return opaque pointer
 */
typedef void * afs_tp_worker_func_t(struct afs_thread_pool * pool,
				    struct afs_thread_pool_worker * worker,
				    struct afs_work_queue * queue,
				    void * rock);

#endif /* AFS_UTIL_THREAD_POOL_TYPES_H */
