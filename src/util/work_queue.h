/*
 * Copyright 2008-2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_UTIL_WORK_QUEUE_H
#define AFS_UTIL_WORK_QUEUE_H 1

#include "work_queue_types.h"

/**
 * public interfaces for package work_queue.
 */
/** @defgroup afs_work_queue Volume Package Work Queue */
/*@{*/

/* XXX move these into an et */
#define AFS_WQ_ERROR              -1 /**< fatal error in work_queue package */
#define AFS_WQ_ERROR_RECOVERABLE  -2 /**< soft error in work_queue package */
#define AFS_WQ_ERROR_RESCHEDULE   -3 /**< reschedule work node for execution */

extern void afs_wq_opts_init(struct afs_work_queue_opts *);
extern void afs_wq_opts_calc_thresh(struct afs_work_queue_opts *, int);
extern int afs_wq_create(struct afs_work_queue **,
                         void * rock,
                         struct afs_work_queue_opts *);
extern int afs_wq_destroy(struct afs_work_queue *);

extern int afs_wq_shutdown(struct afs_work_queue *);

extern int afs_wq_node_alloc(struct afs_work_queue_node **);
extern int afs_wq_node_get(struct afs_work_queue_node *);
extern int afs_wq_node_put(struct afs_work_queue_node *);
extern int afs_wq_node_set_callback(struct afs_work_queue_node *,
				    afs_wq_callback_func_t *,
				    void * rock, afs_wq_callback_dtor_t *dtor);
extern int afs_wq_node_set_detached(struct afs_work_queue_node *);
extern int afs_wq_node_dep_add(struct afs_work_queue_node *,
			       struct afs_work_queue_node *);
extern int afs_wq_node_dep_del(struct afs_work_queue_node *,
			       struct afs_work_queue_node *);
extern int afs_wq_node_block(struct afs_work_queue_node *);
extern int afs_wq_node_unblock(struct afs_work_queue_node *);
extern void afs_wq_add_opts_init(struct afs_work_queue_add_opts *);
extern int afs_wq_add(struct afs_work_queue *,
		      struct afs_work_queue_node *,
		      struct afs_work_queue_add_opts *);
extern int afs_wq_del(struct afs_work_queue_node *);

extern int afs_wq_do(struct afs_work_queue *,
		     void * rock);
extern int afs_wq_do_nowait(struct afs_work_queue *,
			    void * rock);

extern int afs_wq_wait_all(struct afs_work_queue *);
extern int afs_wq_node_wait(struct afs_work_queue_node *,
				 int * retcode);

/*@}*/
#endif /* AFS_UTIL_WORK_QUEUE_H */
