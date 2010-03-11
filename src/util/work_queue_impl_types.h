/*
 * Copyright 2008-2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_UTIL_WORK_QUEUE_IMPL_TYPES_H
#define AFS_UTIL_WORK_QUEUE_IMPL_TYPES_H 1

#ifndef __AFS_WORK_QUEUE_IMPL
#error "do not include this file outside of the work queue implementation"
#endif

#include "work_queue_types.h"
#include <rx/rx_queue.h>

/**
 *  implementation-private type definitions for work_queue.
 */

/**
 * work_queue node state.
 */
typedef enum {
    AFS_WQ_NODE_STATE_INIT,               /**< initial state */
    AFS_WQ_NODE_STATE_SCHEDULED,          /**< scheduled for execution */
    AFS_WQ_NODE_STATE_RUNNING,            /**< running callback function */
    AFS_WQ_NODE_STATE_DONE,               /**< callback function finished */
    AFS_WQ_NODE_STATE_ERROR,              /**< node callback failed, or some dep failed */
    AFS_WQ_NODE_STATE_BLOCKED,            /**< pending some dependency */
    AFS_WQ_NODE_STATE_BUSY,               /**< exclusively owned by a thread */
    /* add new states above this line */
    AFS_WQ_NODE_STATE_TERMINAL
} afs_wq_work_state_t;

/**
 * work_queue dependency node.
 */
struct afs_work_queue_dep_node {
    struct rx_queue parent_list;        /**< parent node's list of children */
    struct afs_work_queue_node * parent;        /**< parent work node */
    struct afs_work_queue_node * child;         /**< child work node */
    /* coming soon: dep options */
};

/**
 * work_queue enumeration.
 *
 * tells which linked list a given node is attached to.
 */
typedef enum {
    AFS_WQ_NODE_LIST_NONE,         /**< node is not on a linked list. */
    AFS_WQ_NODE_LIST_READY,        /**< node is on ready_list */
    AFS_WQ_NODE_LIST_BLOCKED,      /**< node is on blocked_list */
    AFS_WQ_NODE_LIST_DONE,         /**< node is on done_list */
    /* add new queues above this line */
    AFS_WQ_NODE_LIST_TERMINAL
} afs_wq_node_list_id_t;

/**
 * work_queue node.
 */
struct afs_work_queue_node {
    struct rx_queue node_list;          /**< linked list of work queue nodes. */
    afs_wq_node_list_id_t qidx;         /**< id of linked list */
    struct rx_queue dep_children;       /**< nodes whose execution depends upon
					 *   our completion. */
    afs_wq_callback_func_t * cbf;
                                        /**< callback function which will be called by scheduler */
    afs_wq_callback_dtor_t *rock_dtor;  /**< destructor function for 'rock' */
    void * rock;                        /**< opaque pointer passed into cbf */
    struct afs_work_queue * queue;              /**< our queue */
    afs_wq_work_state_t state;          /**< state of this queue node */
    afs_uint32 refcount;                /**< object reference count */
    afs_uint32 block_count;             /**< dependency blocking count; node is
					 *   only a candidate for execution when
					 *   this counter reaches zero. */
    afs_uint32 error_count;             /**< dependency error count; node is only
					 *   a candidate for execution when this
					 *   counter reaches zero. */
    int detached;                       /**< object is put instead of being placed onto done queue */
    int retcode;                        /**< return code from worker function */
    pthread_mutex_t lock;               /**< object lock */
    pthread_cond_t state_cv;            /**< state change cv */
};

/**
 * linked list
 */
struct afs_work_queue_node_list {
    struct rx_queue list;               /**< linked list of nodes */
    afs_wq_node_list_id_t qidx;         /**< id of linked list */
    int shutdown;                       /**< don't allow blocking on dequeue if asserted */
    pthread_mutex_t lock;               /**< synchronize list access */
    pthread_cond_t  cv;                 /**< signal empty->non-empty transition */
};

/**
 * multilock control structure.
 */
struct afs_work_queue_node_multilock {
    struct {
	struct afs_work_queue_node * node;
	int lock_held;
	int busy_held;
    } nodes[2];
};

/**
 * work queue.
 */
struct afs_work_queue {
    struct afs_work_queue_node_list ready_list;     /**< ready work queue nodes. */
    struct afs_work_queue_node_list blocked_list;   /**< nodes scheduled, but blocked */
    struct afs_work_queue_node_list done_list;      /**< nodes done/errored */
    void * rock;                            /**< opaque pointer passed to all callbacks */

    struct afs_work_queue_opts opts;

    int drain;                 /**< 1 if we are waiting for the queue to drain
                                *   until the number of pending tasks has
                                *   dropped below the low threshold */
    int shutdown;              /**< 1 if the queue has been shutdown */
    int pend_count;            /**< number of pending tasks */
    int running_count;         /**< number of tasks busy running */
    pthread_mutex_t lock;      /**< lock for the queue */
    pthread_cond_t pend_cv;    /**< signalled when th queue is draining and
                                *   the number of pending tasks dropped below
                                *   the low threshold */
    pthread_cond_t empty_cv;   /**< signalled when the number of pending tasks
                                *   reaches 0 */
    pthread_cond_t running_cv; /**< signalled once running_count reaches 0 and
                                *   the queue is shutting down */
};

#endif /* AFS_UTIL_WORK_QUEUE_IMPL_TYPES_H */
