/*
 * Copyright 2008-2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_UTIL_THREAD_POOL_IMPL_TYPES_H
#define AFS_UTIL_THREAD_POOL_IMPL_TYPES_H 1

#ifndef __AFS_THREAD_POOL_IMPL
#error "do not include this file outside of the thread pool implementation"
#endif

#include "thread_pool_types.h"
#include <rx/rx_queue.h>

/**
 *
 *  implementation-private type definitions for thread_pool.
 */

/**
 * thread_pool worker state.
 */
typedef enum {
    AFS_TP_STATE_INIT,            /**< initial state */
    AFS_TP_STATE_STARTING,        /**< pool is starting up */
    AFS_TP_STATE_RUNNING,         /**< pool is running normally */
    AFS_TP_STATE_STOPPING,        /**< stop requested */
    AFS_TP_STATE_STOPPED,         /**< pool is shut down */
    /* add new states above this line */
    AFS_TP_STATE_TERMINAL
} afs_tp_state_t;

/**
 * thread_pool worker.
 */
struct afs_thread_pool_worker {
    struct rx_queue worker_list;        /**< linked list of thread workers. */
    struct afs_thread_pool * pool;               /**< associated thread pool */
    void * ret;                         /**< return value from worker thread entry point */
    pthread_t tid;                      /**< thread id */
    int req_shutdown;                   /**< request shutdown of this thread */
};

/**
 * thread pool.
 */
struct afs_thread_pool {
    struct rx_queue thread_list;        /**< linked list of threads */
    struct afs_work_queue * work_queue;         /**< work queue serviced by this thread pool. */
    afs_tp_worker_func_t * entry;       /**< worker thread entry point */
    void * rock;                        /**< opaque pointer passed to worker thread entry point */
    afs_uint32 nthreads;                /**< current pool size */
    afs_tp_state_t state;               /**< pool state */
    afs_uint32 max_threads;             /**< pool options */
    pthread_mutex_t lock;               /**< pool global state lock */
    pthread_cond_t shutdown_cv;         /**< thread shutdown cv */
};

#endif /* AFS_UTIL_THREAD_POOL_IMPL_TYPES_H */
