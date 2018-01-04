/*
 * Copyright 2008-2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_UTIL_WORK_QUEUE_TYPES_H
#define AFS_UTIL_WORK_QUEUE_TYPES_H 1


/**
 *  public type definitions for work_queue.
 */

/* forward declare opaque types */
struct afs_work_queue_node;
struct afs_work_queue;

/**
 * options for creating a struct afs_work_queue.
 *
 * @see afs_wq_create
 */
struct afs_work_queue_opts {
    unsigned int pend_hithresh; /**< maximum number of pending work nodes that
                                 *   can be in the work queue before
                                 *   non-forcing callers of afs_wq_add
                                 *   will block */
    unsigned int pend_lothresh; /**< when the number of pending work nodes
                                 *   falls below this number, any blocked
                                 *   callers of afs_wq_add are woken
                                 *   back up */
};

/**
 * options to afs_wq_add.
 *
 * @see afs_wq_add
 */
struct afs_work_queue_add_opts {
    int donate; /**< 1 to donate the caller's reference of the work node to
                 *   the queue, 0 to add a reference for the queue */
    int block;  /**< 1 to block if the queue is 'full', 0 to return
                 *   immediately with an error if the queue is full. Ignored
                 *   when 'force' is set */
    int force;  /**< 1 to force the node to be added to the queue, even if the
                 *   queue is at or beyond the pend_hithresh quota, 0 to block
                 *   or return an error according to the 'block' option */
};

/**
 * work_queue callback function.
 *
 * @param[in] queue        pointer to struct afs_work_queue
 * @param[in] node         pointer to struct afs_work_queue_node
 * @param[in] queue_rock   opaque pointer associated with this work queue
 * @param[in] node_rock    opaque pointer associated with this work element
 * @param[in] caller_rock  opaque pointer passed in by caller of afs_wq_do()
 *
 * @return operation status
 *    @retval 0 success
 *    @retval AFS_WQ_ERROR callback suffered fatal error;
 *                              propagate to caller immediately.
 *    @retval AFS_WQ_ERROR_RECOVERABLE callback suffered a non-fatal error.
 *    @retval AFS_WQ_ERROR_RESCHEDULE callback requested to be scheduled
 *                                         again; for example it may have
 *                                         changed its dependencies.
 */
typedef int afs_wq_callback_func_t(struct afs_work_queue * queue,
				   struct afs_work_queue_node * node,
				   void * queue_rock,
				   void * node_rock,
				   void * caller_rock);

/**
 * node rock destructor function.
 *
 * This function is called when the associated afs_work_queue_node is freed.
 * It should free any resources associated with the rock.
 *
 * @param[in] node_rock  rock given to the node's callback function
 */
typedef void afs_wq_callback_dtor_t(void *node_rock);

#endif /* AFS_UTIL_WORK_QUEUE_TYPES_H */
