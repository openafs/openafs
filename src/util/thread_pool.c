/*
 * Copyright 2008-2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <afs/opr.h>

#include <lock.h>
#include <afs/afsutil.h>
#include <lwp.h>
#include <afs/afsint.h>

#define __AFS_THREAD_POOL_IMPL 1
#include "work_queue.h"
#include "thread_pool.h"
#include "thread_pool_impl.h"

/**
 * public interfaces for thread_pool.
 */

/**
 * allocate a thread pool object.
 *
 * @param[inout] pool_out address in which to store pool object pointer
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOMEM out of memory
 *
 * @internal
 */
static int
_afs_tp_alloc(struct afs_thread_pool ** pool_out)
{
    int ret = 0;
    struct afs_thread_pool * pool;

    *pool_out = pool = malloc(sizeof(*pool));
    if (pool == NULL) {
	ret = ENOMEM;
	goto error;
    }

 error:
    return ret;
}

/**
 * free a thread pool object.
 *
 * @param[in] pool  thread pool object
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_afs_tp_free(struct afs_thread_pool * pool)
{
    int ret = 0;

    free(pool);

    return ret;
}

/**
 * allocate a thread worker object.
 *
 * @param[inout] worker_out address in which to store worker object pointer
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOMEM out of memory
 *
 * @internal
 */
static int
_afs_tp_worker_alloc(struct afs_thread_pool_worker ** worker_out)
{
    int ret = 0;
    struct afs_thread_pool_worker * worker;

    *worker_out = worker = malloc(sizeof(*worker));
    if (worker == NULL) {
	ret = ENOMEM;
	goto error;
    }

    queue_NodeInit(&worker->worker_list);

 error:
    return ret;
}

/**
 * free a thread worker object.
 *
 * @param[in] worker  thread worker object
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_afs_tp_worker_free(struct afs_thread_pool_worker * worker)
{
    int ret = 0;

    free(worker);

    return ret;
}

/**
 * low-level thread entry point.
 *
 * @param[in] rock opaque pointer to thread worker object
 *
 * @return opaque return pointer from pool entry function
 *
 * @internal
 */
static void *
_afs_tp_worker_run(void * rock)
{
    struct afs_thread_pool_worker * worker = rock;
    struct afs_thread_pool * pool = worker->pool;

    /* register worker with pool */
    opr_mutex_enter(&pool->lock);
    queue_Append(&pool->thread_list, worker);
    pool->nthreads++;
    opr_mutex_exit(&pool->lock);

    /* call high-level entry point */
    worker->ret = (*pool->entry)(pool, worker, pool->work_queue, pool->rock);

    /* adjust pool live thread count */
    opr_mutex_enter(&pool->lock);
    opr_Assert(pool->nthreads);
    queue_Remove(worker);
    pool->nthreads--;
    if (!pool->nthreads) {
	opr_cv_broadcast(&pool->shutdown_cv);
	pool->state = AFS_TP_STATE_STOPPED;
    }
    opr_mutex_exit(&pool->lock);

    _afs_tp_worker_free(worker);

    return NULL;
}

/**
 * default high-level thread entry point.
 *
 * @internal
 */
static void *
_afs_tp_worker_default(struct afs_thread_pool *pool,
                       struct afs_thread_pool_worker *worker,
                       struct afs_work_queue *queue,
                       void *rock)
{
    int code = 0;
    while (code == 0 && afs_tp_worker_continue(worker)) {
	code = afs_wq_do(queue, NULL /* no call rock */);
    }

    return NULL;
}

/**
 * start a worker thread.
 *
 * @param[in] pool         thread pool object
 * @param[inout] worker_out  address in which to store worker thread object pointer
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOMEM out of memory
 */
static int
_afs_tp_worker_start(struct afs_thread_pool * pool,
		     struct afs_thread_pool_worker ** worker_out)
{
    int ret = 0;
    pthread_attr_t attrs;
    struct afs_thread_pool_worker * worker;

    ret = _afs_tp_worker_alloc(worker_out);
    if (ret) {
	goto error;
    }
    worker = *worker_out;

    worker->pool = pool;
    worker->req_shutdown = 0;

    opr_Verify(pthread_attr_init(&attrs) == 0);
    opr_Verify(pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED) == 0);

    ret = pthread_create(&worker->tid, &attrs, &_afs_tp_worker_run, worker);

 error:
    return ret;
}

/**
 * create a thread pool.
 *
 * @param[inout] pool_out  address in which to store pool object pointer.
 * @param[in]    queue     work queue serviced by thread pool
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOMEM out of memory
 */
int
afs_tp_create(struct afs_thread_pool ** pool_out,
	      struct afs_work_queue * queue)
{
    int ret = 0;
    struct afs_thread_pool * pool;

    ret = _afs_tp_alloc(pool_out);
    if (ret) {
	goto error;
    }
    pool = *pool_out;

    opr_mutex_init(&pool->lock);
    opr_cv_init(&pool->shutdown_cv);
    queue_Init(&pool->thread_list);
    pool->work_queue = queue;
    pool->entry = &_afs_tp_worker_default;
    pool->rock = NULL;
    pool->nthreads = 0;
    pool->max_threads = 4;
    pool->state = AFS_TP_STATE_INIT;

 error:
    return ret;
}

/**
 * destroy a thread pool.
 *
 * @param[in] pool thread pool object to be destroyed
 *
 * @return operation status
 *    @retval 0 success
 *    @retval AFS_TP_ERROR pool not in a quiescent state
 */
int
afs_tp_destroy(struct afs_thread_pool * pool)
{
    int ret = 0;

    opr_mutex_enter(&pool->lock);
    switch (pool->state) {
    case AFS_TP_STATE_INIT:
    case AFS_TP_STATE_STOPPED:
	_afs_tp_free(pool);
	break;

    default:
	ret = AFS_TP_ERROR;
	opr_mutex_exit(&pool->lock);
    }

    return ret;
}

/**
 * set the number of threads to spawn.
 *
 * @param[in] pool  thread pool object
 * @param[in] threads  number of threads to spawn
 *
 * @return operation status
 *    @retval 0 success
 *    @retval AFS_TP_ERROR thread pool has already been started
 */
int
afs_tp_set_threads(struct afs_thread_pool *pool,
                   afs_uint32 threads)
{
    int ret = 0;

    opr_mutex_enter(&pool->lock);
    if (pool->state != AFS_TP_STATE_INIT) {
	ret = AFS_TP_ERROR;
    } else {
	pool->max_threads = threads;
    }
    opr_mutex_exit(&pool->lock);

    return ret;
}

/**
 * set a custom thread entry point.
 *
 * @param[in] pool  thread pool object
 * @param[in] entry thread entry function pointer
 * @param[in] rock  opaque pointer passed to thread
 *
 * @return operation status
 *    @retval 0 success
 *    @retval AFS_TP_ERROR thread pool has already been started
 */
int
afs_tp_set_entry(struct afs_thread_pool * pool,
		 afs_tp_worker_func_t * entry,
		 void * rock)
{
    int ret = 0;

    opr_mutex_enter(&pool->lock);
    if (pool->state != AFS_TP_STATE_INIT) {
	ret = AFS_TP_ERROR;
    } else {
	pool->entry = entry;
	pool->rock = rock;
    }
    opr_mutex_exit(&pool->lock);

    return ret;
}

/**
 * start a thread pool.
 *
 * @param[in] pool  thread pool object
 *
 * @return operation status
 *    @retval 0 success
 *    @retval AFS_TP_ERROR thread create failure
 */
int
afs_tp_start(struct afs_thread_pool * pool)
{
    int code, ret = 0;
    struct afs_thread_pool_worker * worker;
    afs_uint32 i;

    opr_mutex_enter(&pool->lock);
    if (pool->state != AFS_TP_STATE_INIT) {
	ret = AFS_TP_ERROR;
	goto done_sync;
    }
    pool->state = AFS_TP_STATE_STARTING;
    opr_mutex_exit(&pool->lock);

    for (i = 0; i < pool->max_threads; i++) {
	code = _afs_tp_worker_start(pool, &worker);
	if (code) {
	    ret = code;
	}
    }

    opr_mutex_enter(&pool->lock);
    pool->state = AFS_TP_STATE_RUNNING;
 done_sync:
    opr_mutex_exit(&pool->lock);

    return ret;
}

/**
 * shut down all threads in pool.
 *
 * @param[in] pool  thread pool object
 * @param[in] block wait for all threads to terminate, if asserted
 *
 * @return operation status
 *    @retval 0 success
 */
int
afs_tp_shutdown(struct afs_thread_pool * pool,
		int block)
{
    int ret = 0;
    struct afs_thread_pool_worker * worker, *nn;

    opr_mutex_enter(&pool->lock);
    if (pool->state == AFS_TP_STATE_STOPPED
        || pool->state == AFS_TP_STATE_STOPPING) {
	goto done_stopped;
    }
    if (pool->state != AFS_TP_STATE_RUNNING) {
	ret = AFS_TP_ERROR;
	goto done_sync;
    }
    pool->state = AFS_TP_STATE_STOPPING;

    for (queue_Scan(&pool->thread_list, worker, nn, afs_thread_pool_worker)) {
	worker->req_shutdown = 1;
    }
    if (!pool->nthreads) {
	pool->state = AFS_TP_STATE_STOPPED;
    }
    /* need to drop lock to get a membar here */
    opr_mutex_exit(&pool->lock);

    ret = afs_wq_shutdown(pool->work_queue);
    if (ret) {
	goto error;
    }

    opr_mutex_enter(&pool->lock);
 done_stopped:
    if (block) {
	while (pool->nthreads) {
	    opr_cv_wait(&pool->shutdown_cv, &pool->lock);
	}
    }
 done_sync:
    opr_mutex_exit(&pool->lock);

 error:
    return ret;
}

/**
 * check whether thread pool is online.
 *
 * @param[in] pool  thread pool object
 *
 * @return whether pool is online
 *    @retval 1 pool is online
 *    @retval 0 pool is not online
 */
int
afs_tp_is_online(struct afs_thread_pool * pool)
{
    int ret;

    opr_mutex_enter(&pool->lock);
    ret = (pool->state == AFS_TP_STATE_RUNNING);
    opr_mutex_exit(&pool->lock);

    return ret;
}

/**
 * check whether a given worker thread can continue to run.
 *
 * @param[in] worker  worker thread object pointer
 *
 * @return whether thread can continue to execute
 *    @retval 1 execution can continue
 *    @retval 0 shutdown has been requested
 */
int
afs_tp_worker_continue(struct afs_thread_pool_worker * worker)
{
    return !worker->req_shutdown;
}
