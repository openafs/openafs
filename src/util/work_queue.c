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
#include <sys/file.h>

#include <lock.h>

#define __AFS_WORK_QUEUE_IMPL 1
#include "work_queue.h"
#include "work_queue_impl.h"

/**
 * public interfaces for work_queue.
 */

static int
_afs_wq_node_put_r(struct afs_work_queue_node * node,
		   int drop);

/**
 * allocate a work queue object.
 *
 * @param[out] queue_out address in which to store queue pointer
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOMEM out of memory
 *
 * @internal
 */
static int
_afs_wq_alloc(struct afs_work_queue ** queue_out)
{
    int ret = 0;
    struct afs_work_queue * queue;

    *queue_out = queue = malloc(sizeof(*queue));
    if (queue == NULL) {
	ret = ENOMEM;
	goto error;
    }

 error:
    return ret;
}

/**
 * free a work queue object.
 *
 * @param[in] queue  work queue object
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_afs_wq_free(struct afs_work_queue * queue)
{
    int ret = 0;

    free(queue);

    return ret;
}

/**
 * change a node's state.
 *
 * @param[in] node       node object
 * @param[in] new_state  new object state
 *
 * @return old state
 *
 * @pre node->lock held
 *
 * @internal
 */
static afs_wq_work_state_t
_afs_wq_node_state_change(struct afs_work_queue_node * node,
			  afs_wq_work_state_t new_state)
{
    afs_wq_work_state_t old_state;

    old_state = node->state;
    node->state = new_state;

    CV_BROADCAST(&node->state_cv);

    return old_state;
}

/**
 * wait for a node's state to change from busy to something else.
 *
 * @param[in] node  node object
 *
 * @return operation status
 *    @retval 0 success
 *
 * @pre node->lock held
 *
 * @internal
 */
static int
_afs_wq_node_state_wait_busy(struct afs_work_queue_node * node)
{
    while (node->state == AFS_WQ_NODE_STATE_BUSY) {
	CV_WAIT(&node->state_cv, &node->lock);
    }

    return 0;
}

/**
 * check whether a work queue node is busy.
 *
 * @param[in] node  node object pointer
 *
 * @return whether node is busy
 *    @retval 1 node is busy
 *    @retval 0 node is not busy
 *
 * @pre node->lock held
 *
 * @internal
 */
static int
_afs_wq_node_state_is_busy(struct afs_work_queue_node * node)
{
    return (node->state == AFS_WQ_NODE_STATE_BUSY);
}

/**
 * attempt to simultaneously lock two work queue nodes.
 *
 * this is a somewhat tricky algorithm because there is no
 * defined hierarchy within the work queue node population.
 *
 * @param[in] ml  multilock control structure
 *
 * @return operation status
 *    @retval 0
 *
 * @note in theory, we could easily extend this to
 *       lock more than two nodes
 *
 * @pre
 *   - caller MUST NOT have set busy state on either node
 *
 * @post
 *   - locks held on both nodes
 *   - both nodes in quiescent states
 *
 * @note node with non-zero lock_held or busy_held fields
 *       MUST go in array index 0
 *
 * @internal
 */
static int
_afs_wq_node_multilock(struct afs_work_queue_node_multilock * ml)
{
    int code, ret = 0;
    struct timespec delay;
    int first = 1, second = 0, tmp;

    /* first pass processing */
    if (ml->nodes[0].lock_held) {
	if (!ml->nodes[0].busy_held) {
	    ret = _afs_wq_node_state_wait_busy(ml->nodes[0].node);
	    if (ret) {
		goto error;
	    }
	}

	code = MUTEX_TRYENTER(&ml->nodes[1].node->lock);
	if (code) {
	    /* success */
	    goto done;
	}

	/* setup for main loop */
	MUTEX_EXIT(&ml->nodes[0].node->lock);
    }

    /*
     * setup random exponential backoff
     *
     * set initial delay to random value in the range [500,1000) ns
     */
    delay.tv_sec = 0;
    delay.tv_nsec = 500 + rand() % 500;

    while (1) {
	MUTEX_ENTER(&ml->nodes[first].node->lock);
	if ((first != 0) || !ml->nodes[0].busy_held) {
	    ret = _afs_wq_node_state_wait_busy(ml->nodes[first].node);
	    if (ret) {
		/* cleanup */
		if (!ml->nodes[0].lock_held || first) {
		    MUTEX_EXIT(&ml->nodes[first].node->lock);
		    if (ml->nodes[0].lock_held) {
			/* on error, return with locks in same state as before call */
			MUTEX_ENTER(&ml->nodes[0].node->lock);
		    }
		}
		goto error;
	    }
	}

	/*
	 * in order to avoid deadlock, we must use trylock and
	 * a non-blocking state check.  if we meet any contention,
	 * we must drop back and start again.
	 */
	code = MUTEX_TRYENTER(&ml->nodes[second].node->lock);
	if (code) {
	    if (((second == 0) && (ml->nodes[0].busy_held)) ||
		!_afs_wq_node_state_is_busy(ml->nodes[second].node)) {
		/* success */
		break;
	    } else {
		MUTEX_EXIT(&ml->nodes[second].node->lock);
	    }
	}

	/*
	 * contended.
	 *
	 * drop locks, use exponential backoff,
	 * try acquiring in the opposite order
	 */
	MUTEX_EXIT(&ml->nodes[first].node->lock);
	nanosleep(&delay, NULL);
	if (delay.tv_nsec <= 65536000) { /* max backoff delay of ~131ms */
	    delay.tv_nsec <<= 1;
	}
	tmp = second;
	second = first;
	first = tmp;
    }

 done:
 error:
    return ret;
}

/**
 * initialize a node list object.
 *
 * @param[in] list list object
 * @param[in] id   list identifier
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_afs_wq_node_list_init(struct afs_work_queue_node_list * list,
		       afs_wq_node_list_id_t id)
{
    queue_Init(&list->list);
    MUTEX_INIT(&list->lock, "list", MUTEX_DEFAULT, 0);
    CV_INIT(&list->cv, "list", CV_DEFAULT, 0);
    list->qidx = id;
    list->shutdown = 0;

    return 0;
}

/**
 * destroy a node list object.
 *
 * @param[in] list list object
 *
 * @return operation status
 *    @retval 0 success
 *    @retval AFS_WQ_ERROR list not empty
 *
 * @internal
 */
static int
_afs_wq_node_list_destroy(struct afs_work_queue_node_list * list)
{
    int ret = 0;

    if (queue_IsNotEmpty(&list->list)) {
	ret = AFS_WQ_ERROR;
	goto error;
    }

    MUTEX_DESTROY(&list->lock);
    CV_DESTROY(&list->cv);

 error:
    return ret;
}

/**
 * wakeup all threads waiting in dequeue.
 *
 * @param[in] list list object
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_afs_wq_node_list_shutdown(struct afs_work_queue_node_list * list)
{
    int ret = 0;
    struct afs_work_queue_node *node, *nnode;

    MUTEX_ENTER(&list->lock);
    list->shutdown = 1;

    for (queue_Scan(&list->list, node, nnode, afs_work_queue_node)) {
	_afs_wq_node_state_change(node, AFS_WQ_NODE_STATE_ERROR);
	queue_Remove(node);
	node->qidx = AFS_WQ_NODE_LIST_NONE;
	node->queue = NULL;

	if (node->detached) {
	    /* if we are detached, we hold the reference on the node;
	     * otherwise, it is some other caller that holds the reference.
	     * So don't put the node if we are not detached; the node will
	     * get freed when someone else calls afs_wq_node_put */
	    afs_wq_node_put(node);
	}
    }

    CV_BROADCAST(&list->cv);
    MUTEX_EXIT(&list->lock);

    return ret;
}

/**
 * append to a node list object.
 *
 * @param[in] list  list object
 * @param[in] node  node object
 * @param[in] state new node state
 *
 * @return operation status
 *    @retval 0 success
 *    @retval AFS_WQ_ERROR raced to enqueue node
 *
 * @pre
 *   - node lock held
 *   - node is not on a list
 *   - node is either not busy, or it is marked as busy by the calling thread
 *
 * @post
 *   - enqueued on list
 *   - node lock dropped
 *
 * @internal
 */
static int
_afs_wq_node_list_enqueue(struct afs_work_queue_node_list * list,
			  struct afs_work_queue_node * node,
			  afs_wq_work_state_t state)
{
    int code, ret = 0;

    if (node->qidx != AFS_WQ_NODE_LIST_NONE) {
	/* raced */
	ret = AFS_WQ_ERROR;
	goto error;
    }

    /* deal with lock inversion */
    code = MUTEX_TRYENTER(&list->lock);
    if (!code) {
	/* contended */
	_afs_wq_node_state_change(node, AFS_WQ_NODE_STATE_BUSY);
	MUTEX_EXIT(&node->lock);
	MUTEX_ENTER(&list->lock);
	MUTEX_ENTER(&node->lock);

	/* assert state of the world (we set busy, so this should never happen) */
	osi_Assert(queue_IsNotOnQueue(node));
    }

    if (list->shutdown) {
	ret = AFS_WQ_ERROR;
	goto error_unlock;
    }

    osi_Assert(node->qidx == AFS_WQ_NODE_LIST_NONE);
    if (queue_IsEmpty(&list->list)) {
	/* wakeup a dequeue thread */
	CV_SIGNAL(&list->cv);
    }
    queue_Append(&list->list, node);
    node->qidx = list->qidx;
    _afs_wq_node_state_change(node, state);

 error_unlock:
    MUTEX_EXIT(&node->lock);
    MUTEX_EXIT(&list->lock);

 error:
    return ret;
}

/**
 * dequeue a node from a list object.
 *
 * @param[in]    list      list object
 * @param[out]   node_out  address in which to store node object pointer
 * @param[in]    state     new node state
 * @param[in]    block     permit blocking on cv if asserted
 *
 * @return operation status
 *    @retval 0 success
 *    @retval EWOULDBLOCK block not asserted and nothing to dequeue
 *    @retval EINTR blocking wait interrupted by list shutdown
 *
 * @post node object returned with node lock held and new state set
 *
 * @internal
 */
static int
_afs_wq_node_list_dequeue(struct afs_work_queue_node_list * list,
			  struct afs_work_queue_node ** node_out,
			  afs_wq_work_state_t state,
			  int block)
{
    int ret = 0;
    struct afs_work_queue_node * node;

    MUTEX_ENTER(&list->lock);

    if (list->shutdown) {
	*node_out = NULL;
	ret = EINTR;
	goto done_sync;
    }

    if (!block && queue_IsEmpty(&list->list)) {
	*node_out = NULL;
	ret = EWOULDBLOCK;
	goto done_sync;
    }

    while (queue_IsEmpty(&list->list)) {
	if (list->shutdown) {
	    *node_out = NULL;
	    ret = EINTR;
	    goto done_sync;
	}
	CV_WAIT(&list->cv, &list->lock);
    }

    *node_out = node = queue_First(&list->list, afs_work_queue_node);

    MUTEX_ENTER(&node->lock);
    queue_Remove(node);
    node->qidx = AFS_WQ_NODE_LIST_NONE;
    _afs_wq_node_state_change(node, state);

 done_sync:
    MUTEX_EXIT(&list->lock);

    return ret;
}

/**
 * remove a node from a list.
 *
 * @param[in] node        node object
 * @param[in] next_state  node state following successful dequeue
 *
 * @return operation status
 *    @retval 0 success
 *    @retval AFS_WQ_ERROR in any of the following conditions:
 *              - node not associated with a work queue
 *              - node was not on a linked list (e.g. RUNNING state)
 *              - we raced another thread
 *
 * @pre node->lock held
 *
 * @post node removed from node list
 *
 * @note node->lock may be dropped internally
 *
 * @internal
 */
static int
_afs_wq_node_list_remove(struct afs_work_queue_node * node,
			 afs_wq_work_state_t next_state)
{
    int code, ret = 0;
    struct afs_work_queue_node_list * list = NULL;

    _afs_wq_node_state_wait_busy(node);

    if (!node->queue) {
	ret = AFS_WQ_ERROR;
	goto error;
    }
    switch (node->qidx) {
    case AFS_WQ_NODE_LIST_READY:
	list = &node->queue->ready_list;
	break;

    case AFS_WQ_NODE_LIST_BLOCKED:
	list = &node->queue->blocked_list;
	break;

    case AFS_WQ_NODE_LIST_DONE:
	list = &node->queue->done_list;
	break;

    default:
	ret = AFS_WQ_ERROR;
    }

    if (list) {
	code = MUTEX_TRYENTER(&list->lock);
	if (!code) {
	    /* contended */
	    _afs_wq_node_state_change(node,
					   AFS_WQ_NODE_STATE_BUSY);
	    MUTEX_EXIT(&node->lock);
	    MUTEX_ENTER(&list->lock);
	    MUTEX_ENTER(&node->lock);

	    if (node->qidx == AFS_WQ_NODE_LIST_NONE) {
		/* raced */
		ret= AFS_WQ_ERROR;
		goto done_sync;
	    }
	}

	queue_Remove(node);
	node->qidx = AFS_WQ_NODE_LIST_NONE;
	_afs_wq_node_state_change(node, next_state);

    done_sync:
	MUTEX_EXIT(&list->lock);
    }

 error:
    return ret;
}

/**
 * allocate a dependency node.
 *
 * @param[out] node_out address in which to store dep node pointer
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOMEM   out of memory
 *
 * @internal
 */
static int
_afs_wq_dep_alloc(struct afs_work_queue_dep_node ** node_out)
{
    int ret = 0;
    struct afs_work_queue_dep_node * node;

    node = malloc(sizeof(*node));
    if (node == NULL) {
	ret = ENOMEM;
	goto error;
    }

    queue_NodeInit(&node->parent_list);
    node->parent = node->child = NULL;

    *node_out = node;

 error:
    return ret;
}

/**
 * free a dependency node.
 *
 * @param[in] node dep node pointer
 *
 * @return operation status
 *    @retval 0 success
 *    @retval AFS_WQ_ERROR still attached to a work node
 *
 * @internal
 */
static int
_afs_wq_dep_free(struct afs_work_queue_dep_node * node)
{
    int ret = 0;

    if (queue_IsOnQueue(&node->parent_list) ||
	node->parent ||
	node->child) {
	ret = AFS_WQ_ERROR;
	goto error;
    }

    free(node);

 error:
    return ret;
}

/**
 * unlink work nodes from a dependency node.
 *
 * @param[in]  dep      dependency node
 *
 * @return operation status
 *    @retval 0 success
 *
 * @pre
 *   - dep->parent and dep->child are either locked, or are not referenced
 *     by anything else
 *   - caller holds ref on dep->child
 *   - dep->child and dep->parent in quiescent state
 *
 * @internal
 */
static int
_afs_wq_dep_unlink_r(struct afs_work_queue_dep_node *dep)
{
    struct afs_work_queue_node *child = dep->child;
    queue_Remove(&dep->parent_list);
    dep->child = NULL;
    dep->parent = NULL;

    return _afs_wq_node_put_r(child, 0);
}

/**
 * get a reference to a work node.
 *
 * @param[in] node  work queue node
 *
 * @return operation status
 *    @retval 0 success
 *
 * @pre node->lock held
 *
 * @internal
 */
static int
_afs_wq_node_get_r(struct afs_work_queue_node * node)
{
    node->refcount++;

    return 0;
}

/**
 * unlink and free all of the dependency nodes from a node.
 *
 * @param[in] parent  work node that is the parent node of all deps to be freed
 *
 * @return operation status
 *  @retval 0 success
 *
 * @pre parent->refcount == 0
 */
static int
_afs_wq_node_free_deps(struct afs_work_queue_node *parent)
{
    int ret = 0, code;
    struct afs_work_queue_node *node_unlock = NULL, *node_put = NULL;
    struct afs_work_queue_dep_node * dep, * nd;

    /* unlink and free all of the dep structs attached to 'parent' */
    for (queue_Scan(&parent->dep_children,
		    dep,
		    nd,
		    afs_work_queue_dep_node)) {

	MUTEX_ENTER(&dep->child->lock);
	node_unlock = dep->child;

	/* We need to get a ref on child here, since _afs_wq_dep_unlink_r may
	 * put the last ref on the child, and we need the child to still exist
	 * so we can unlock it */
	code = _afs_wq_node_get_r(dep->child);
	if (code) {
	    goto loop_error;
	}
	node_put = dep->child;

	/* remember, no need to lock dep->parent, since its refcount is 0 */
	code = _afs_wq_dep_unlink_r(dep);

     loop_error:
	if (node_put) {
	    _afs_wq_node_put_r(node_put, 1);
	} else if (node_unlock) {
	    MUTEX_EXIT(&node_unlock->lock);
	}
	node_put = node_unlock = NULL;

	if (code == 0) {
	    /* Only do this if everything is okay; if code is nonzero,
	     * something will still be pointing at dep, so don't free it.
	     * We will leak memory, but that's better than memory corruption;
	     * we've done all we can do to try and free the dep memory */
	    code = _afs_wq_dep_free(dep);
	}

	if (!ret) {
	    ret = code;
	}
    }
    return ret;
}

/**
 * propagate state down through dep nodes.
 *
 * @param[in] parent      parent node object
 * @param[in] next_state  next state parent will assume
 *
 * @return operation status
 *    @retval 0 success
 *
 * @pre
 *   - parent->lock held
 *
 * @internal
 */
static int
_afs_wq_dep_propagate(struct afs_work_queue_node * parent,
		      afs_wq_work_state_t next_state)
{
    int ret = 0;
    struct afs_work_queue_dep_node * dep, * nd;
    struct afs_work_queue_node_multilock ml;
    afs_wq_work_state_t old_state;
    afs_wq_node_list_id_t qidx;
    struct afs_work_queue_node_list * ql;
    afs_wq_work_state_t cns;

    old_state = _afs_wq_node_state_change(parent,
					       AFS_WQ_NODE_STATE_BUSY);
    ml.nodes[0].node = parent;
    ml.nodes[0].lock_held = 1;
    ml.nodes[0].busy_held = 1;

    /* scan through our children updating scheduling state */
    for (queue_Scan(&parent->dep_children,
		    dep,
		    nd,
		    afs_work_queue_dep_node)) {
	/* skip half-registered nodes */
	if (dep->child == NULL) {
	    continue;
	}

	ml.nodes[1].node = dep->child;
	ml.nodes[1].lock_held = 0;
	ml.nodes[1].busy_held = 0;
	ret = _afs_wq_node_multilock(&ml);
	if (ret) {
	    goto error;
	}

	switch (next_state) {
	case AFS_WQ_NODE_STATE_DONE:
	    dep->child->block_count--;
	    break;

	case AFS_WQ_NODE_STATE_ERROR:
	    dep->child->error_count++;
	    break;

	default:
	    (void)0; /* nop */
	}

	/* skip unscheduled nodes */
	if (dep->child->queue == NULL) {
	    MUTEX_EXIT(&dep->child->lock);
	    continue;
	}

	/*
	 * when blocked dep and error'd dep counts reach zero, the
	 * node can be scheduled for execution
	 */
	if (dep->child->error_count) {
	    ql = &dep->child->queue->done_list;
	    qidx = AFS_WQ_NODE_LIST_DONE;
	    cns = AFS_WQ_NODE_STATE_ERROR;
	} else if (dep->child->block_count) {
	    ql = &dep->child->queue->blocked_list;
	    qidx = AFS_WQ_NODE_LIST_BLOCKED;
	    cns = AFS_WQ_NODE_STATE_BLOCKED;
	} else {
	    ql = &dep->child->queue->ready_list;
	    qidx = AFS_WQ_NODE_LIST_READY;
	    cns = AFS_WQ_NODE_STATE_SCHEDULED;
	}

	if (qidx != dep->child->qidx) {
	    /* we're transitioning to a different queue */
	    ret = _afs_wq_node_list_remove(dep->child,
						AFS_WQ_NODE_STATE_BUSY);
	    if (ret) {
		MUTEX_EXIT(&dep->child->lock);
		goto error;
	    }

	    ret = _afs_wq_node_list_enqueue(ql,
						 dep->child,
						 cns);
	    if (ret) {
		MUTEX_EXIT(&dep->child->lock);
		goto error;
	    }
	}
	MUTEX_EXIT(&dep->child->lock);
    }

 error:
    _afs_wq_node_state_change(parent,
				   old_state);
    return ret;
}

/**
 * decrements queue->running_count, and signals waiters if appropriate.
 *
 * @param[in] queue  queue to dec the running count of
 */
static void
_afs_wq_dec_running_count(struct afs_work_queue *queue)
{
    MUTEX_ENTER(&queue->lock);
    queue->running_count--;
    if (queue->shutdown && queue->running_count == 0) {
	/* if we've shut down, someone may be waiting for the running count
	 * to drop to 0 */
	CV_BROADCAST(&queue->running_cv);
    }
    MUTEX_EXIT(&queue->lock);
}

/**
 * execute a node on the queue.
 *
 * @param[in] queue  work queue
 * @param[in] rock   opaque pointer (passed as third arg to callback func)
 * @param[in] block  allow blocking in dequeue
 *
 * @return operation status
 *    @retval 0 completed a work unit
 *
 * @internal
 */
static int
_afs_wq_do(struct afs_work_queue * queue,
	   void * rock,
	   int block)
{
    int code, ret = 0;
    struct afs_work_queue_node * node;
    afs_wq_callback_func_t * cbf;
    afs_wq_work_state_t next_state;
    struct afs_work_queue_node_list * ql;
    void * node_rock;
    int detached = 0;

    /* We can inc queue->running_count before actually pulling the node off
     * of the ready_list, since running_count only really matters when we are
     * shut down. If we get shut down before we pull the node off of
     * ready_list, but after we inc'd running_count,
     * _afs_wq_node_list_dequeue should return immediately with EINTR,
     * in which case we'll dec running_count, so it's as if we never inc'd it
     * in the first place. */
    MUTEX_ENTER(&queue->lock);
    if (queue->shutdown) {
	MUTEX_EXIT(&queue->lock);
	return EINTR;
    }
    queue->running_count++;
    MUTEX_EXIT(&queue->lock);

    ret = _afs_wq_node_list_dequeue(&queue->ready_list,
					 &node,
					 AFS_WQ_NODE_STATE_RUNNING,
					 block);
    if (ret) {
	_afs_wq_dec_running_count(queue);
	goto error;
    }

    cbf = node->cbf;
    node_rock = node->rock;
    detached = node->detached;

    if (cbf != NULL) {
	MUTEX_EXIT(&node->lock);
	code = (*cbf)(queue, node, queue->rock, node_rock, rock);
	MUTEX_ENTER(&node->lock);
	if (code == 0) {
	    next_state = AFS_WQ_NODE_STATE_DONE;
	    ql = &queue->done_list;
	} else if (code == AFS_WQ_ERROR_RESCHEDULE) {
	    if (node->error_count) {
		next_state = AFS_WQ_NODE_STATE_ERROR;
		ql = &queue->done_list;
	    } else if (node->block_count) {
		next_state = AFS_WQ_NODE_STATE_BLOCKED;
		ql = &queue->blocked_list;
	    } else {
		next_state = AFS_WQ_NODE_STATE_SCHEDULED;
		ql = &queue->ready_list;
	    }
	} else {
	    next_state = AFS_WQ_NODE_STATE_ERROR;
	    ql = &queue->done_list;
	}
    } else {
	next_state = AFS_WQ_NODE_STATE_DONE;
	code = 0;
	ql = &queue->done_list;
    }

    _afs_wq_dec_running_count(queue);

    node->retcode = code;

    if ((next_state == AFS_WQ_NODE_STATE_DONE) ||
        (next_state == AFS_WQ_NODE_STATE_ERROR)) {

	MUTEX_ENTER(&queue->lock);

	if (queue->drain && queue->pend_count == queue->opts.pend_lothresh) {
	    /* signal other threads if we're about to below the low
	     * pending-tasks threshold */
	    queue->drain = 0;
	    CV_SIGNAL(&queue->pend_cv);
	}

	if (queue->pend_count == 1) {
	    /* signal other threads if we're about to become 'empty' */
	    CV_BROADCAST(&queue->empty_cv);
	}

	queue->pend_count--;

	MUTEX_EXIT(&queue->lock);
    }

    ret = _afs_wq_node_state_wait_busy(node);
    if (ret) {
	goto error;
    }

    /* propagate scheduling changes down through dependencies */
    ret = _afs_wq_dep_propagate(node, next_state);
    if (ret) {
	goto error;
    }

    ret = _afs_wq_node_state_wait_busy(node);
    if (ret) {
	goto error;
    }

    if (detached &&
	((next_state == AFS_WQ_NODE_STATE_DONE) ||
	 (next_state == AFS_WQ_NODE_STATE_ERROR))) {
	_afs_wq_node_state_change(node, next_state);
	_afs_wq_node_put_r(node, 1);
    } else {
	ret = _afs_wq_node_list_enqueue(ql,
					     node,
					     next_state);
    }

 error:
    return ret;
}

/**
 * initialize a struct afs_work_queue_opts to the default values
 *
 * @param[out] opts  opts struct to initialize
 */
void
afs_wq_opts_init(struct afs_work_queue_opts *opts)
{
    opts->pend_lothresh = 0;
    opts->pend_hithresh = 0;
}

/**
 * set the options for a struct afs_work_queue_opts appropriate for a certain
 * number of threads.
 *
 * @param[out] opts   opts struct in which to set the values
 * @param[in] threads number of threads
 */
void
afs_wq_opts_calc_thresh(struct afs_work_queue_opts *opts, int threads)
{
    opts->pend_lothresh = threads * 2;
    opts->pend_hithresh = threads * 16;

    /* safety */
    if (opts->pend_lothresh < 1) {
	opts->pend_lothresh = 1;
    }
    if (opts->pend_hithresh < 2) {
	opts->pend_hithresh = 2;
    }
}

/**
 * allocate and initialize a work queue object.
 *
 * @param[out]   queue_out  address in which to store newly allocated work queue object
 * @param[in]    rock       work queue opaque pointer (passed as first arg to all fired callbacks)
 * @param[in]    opts       options for the new created queue
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOMEM         out of memory
 */
int
afs_wq_create(struct afs_work_queue ** queue_out,
	      void * rock,
              struct afs_work_queue_opts *opts)
{
    int ret = 0;
    struct afs_work_queue * queue;

    ret = _afs_wq_alloc(queue_out);
    if (ret) {
	goto error;
    }
    queue = *queue_out;

    if (opts) {
	memcpy(&queue->opts, opts, sizeof(queue->opts));
    } else {
	afs_wq_opts_init(&queue->opts);
    }

    _afs_wq_node_list_init(&queue->ready_list,
				AFS_WQ_NODE_LIST_READY);
    _afs_wq_node_list_init(&queue->blocked_list,
				AFS_WQ_NODE_LIST_BLOCKED);
    _afs_wq_node_list_init(&queue->done_list,
				AFS_WQ_NODE_LIST_DONE);
    queue->rock = rock;
    queue->drain = 0;
    queue->shutdown = 0;
    queue->pend_count = 0;
    queue->running_count = 0;

    MUTEX_INIT(&queue->lock, "queue", MUTEX_DEFAULT, 0);
    CV_INIT(&queue->pend_cv, "queue pending", CV_DEFAULT, 0);
    CV_INIT(&queue->empty_cv, "queue empty", CV_DEFAULT, 0);
    CV_INIT(&queue->running_cv, "queue running", CV_DEFAULT, 0);

 error:
    return ret;
}

/**
 * deallocate and free a work queue object.
 *
 * @param[in] queue  work queue to be destroyed
 *
 * @return operation status
 *    @retval 0  success
 *    @retval AFS_WQ_ERROR unspecified error
 */
int
afs_wq_destroy(struct afs_work_queue * queue)
{
    int ret = 0;

    ret = _afs_wq_node_list_destroy(&queue->ready_list);
    if (ret) {
	goto error;
    }

    ret = _afs_wq_node_list_destroy(&queue->blocked_list);
    if (ret) {
	goto error;
    }

    ret = _afs_wq_node_list_destroy(&queue->done_list);
    if (ret) {
	goto error;
    }

    ret = _afs_wq_free(queue);

 error:
    return ret;
}

/**
 * shutdown a work queue.
 *
 * @param[in] queue work queue object pointer
 *
 * @return operation status
 *    @retval 0 success
 */
int
afs_wq_shutdown(struct afs_work_queue * queue)
{
    int ret = 0;

    MUTEX_ENTER(&queue->lock);
    if (queue->shutdown) {
	/* already shutdown, do nothing */
	MUTEX_EXIT(&queue->lock);
	goto error;
    }
    queue->shutdown = 1;

    ret = _afs_wq_node_list_shutdown(&queue->ready_list);
    if (ret) {
	goto error;
    }

    ret = _afs_wq_node_list_shutdown(&queue->blocked_list);
    if (ret) {
	goto error;
    }

    ret = _afs_wq_node_list_shutdown(&queue->done_list);
    if (ret) {
	goto error;
    }

    /* signal everyone that could be waiting, since these conditions will
     * generally fail to signal on their own if we're shutdown, since no
     * progress is being made */
    CV_BROADCAST(&queue->pend_cv);
    CV_BROADCAST(&queue->empty_cv);
    MUTEX_EXIT(&queue->lock);

 error:
    return ret;
}

/**
 * allocate a work node.
 *
 * @param[out] node_out  address in which to store new work node
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOMEM         out of memory
 */
int
afs_wq_node_alloc(struct afs_work_queue_node ** node_out)
{
    int ret = 0;
    struct afs_work_queue_node * node;

    *node_out = node = malloc(sizeof(*node));
    if (node == NULL) {
	ret = ENOMEM;
	goto error;
    }

    queue_NodeInit(&node->node_list);
    node->qidx = AFS_WQ_NODE_LIST_NONE;
    node->cbf = NULL;
    node->rock = node->queue = NULL;
    node->refcount = 1;
    node->block_count = 0;
    node->error_count = 0;
    MUTEX_INIT(&node->lock, "node", MUTEX_DEFAULT, 0);
    CV_INIT(&node->state_cv, "node state", CV_DEFAULT, 0);
    node->state = AFS_WQ_NODE_STATE_INIT;
    queue_Init(&node->dep_children);

 error:
    return ret;
}

/**
 * free a work node.
 *
 * @param[in] node  work node object
 *
 * @return operation status
 *    @retval 0 success
 *
 * @internal
 */
static int
_afs_wq_node_free(struct afs_work_queue_node * node)
{
    int ret = 0;

    if (queue_IsOnQueue(node) ||
	(node->state == AFS_WQ_NODE_STATE_SCHEDULED) ||
	(node->state == AFS_WQ_NODE_STATE_RUNNING) ||
	(node->state == AFS_WQ_NODE_STATE_BLOCKED)) {
	ret = AFS_WQ_ERROR;
	goto error;
    }

    ret = _afs_wq_node_free_deps(node);
    if (ret) {
	goto error;
    }

    MUTEX_DESTROY(&node->lock);
    CV_DESTROY(&node->state_cv);

    if (node->rock_dtor) {
	(*node->rock_dtor) (node->rock);
    }

    free(node);

 error:
    return ret;
}

/**
 * get a reference to a work node.
 *
 * @param[in] node  work queue node
 *
 * @return operation status
 *    @retval 0 success
 */
int
afs_wq_node_get(struct afs_work_queue_node * node)
{
    MUTEX_ENTER(&node->lock);
    node->refcount++;
    MUTEX_EXIT(&node->lock);

    return 0;
}

/**
 * put back a reference to a work node.
 *
 * @param[in] node  work queue node
 * @param[in] drop  drop node->lock
 *
 * @post if refcount reaches zero, node is deallocated.
 *
 * @return operation status
 *    @retval 0 success
 *
 * @pre node->lock held
 *
 * @internal
 */
static int
_afs_wq_node_put_r(struct afs_work_queue_node * node,
		   int drop)
{
    afs_uint32 refc;

    osi_Assert(node->refcount > 0);
    refc = --node->refcount;
    if (drop) {
	MUTEX_EXIT(&node->lock);
    }
    if (!refc) {
	osi_Assert(node->qidx == AFS_WQ_NODE_LIST_NONE);
	_afs_wq_node_free(node);
    }

    return 0;
}

/**
 * put back a reference to a work node.
 *
 * @param[in] node  work queue node
 *
 * @post if refcount reaches zero, node is deallocated.
 *
 * @return operation status
 *    @retval 0 success
 */
int
afs_wq_node_put(struct afs_work_queue_node * node)
{
    MUTEX_ENTER(&node->lock);
    return _afs_wq_node_put_r(node, 1);
}

/**
 * set the callback function on a work node.
 *
 * @param[in] node  work queue node
 * @param[in] cbf   callback function
 * @param[in] rock  opaque pointer passed to callback
 * @param[in] rock_dtor  destructor function for 'rock', or NULL
 *
 * @return operation status
 *    @retval 0 success
 */
int
afs_wq_node_set_callback(struct afs_work_queue_node * node,
			 afs_wq_callback_func_t * cbf,
			 void * rock, afs_wq_callback_dtor_t *dtor)
{
    MUTEX_ENTER(&node->lock);
    node->cbf = cbf;
    node->rock = rock;
    node->rock_dtor = dtor;
    MUTEX_EXIT(&node->lock);

    return 0;
}

/**
 * detach work node.
 *
 * @param[in] node  work queue node
 *
 * @return operation status
 *    @retval 0 success
 */
int
afs_wq_node_set_detached(struct afs_work_queue_node * node)
{
    MUTEX_ENTER(&node->lock);
    node->detached = 1;
    MUTEX_EXIT(&node->lock);

    return 0;
}

/**
 * link a dependency node to a parent and child work node.
 *
 * This links a dependency node such that when the 'parent' work node is
 * done, the 'child' work node can proceed.
 *
 * @param[in]  dep      dependency node
 * @param[in]  parent   parent node in this dependency
 * @param[in]  child    child node in this dependency
 *
 * @return operation status
 *    @retval 0 success
 *
 * @pre
 *   - parent->lock held
 *   - child->lock held
 *   - parent and child in quiescent state
 *
 * @internal
 */
static int
_afs_wq_dep_link_r(struct afs_work_queue_dep_node *dep,
                   struct afs_work_queue_node *parent,
                   struct afs_work_queue_node *child)
{
    int ret = 0;

    /* Each dep node adds a ref to the child node of that dep. We do not
     * do the same for the parent node, since if the only refs remaining
     * for a node are deps in node->dep_children, then the node should be
     * destroyed, and we will destroy the dep nodes when we free the
     * work node. */
    ret = _afs_wq_node_get_r(child);
    if (ret) {
	goto error;
    }

    /* add this dep node to the parent node's list of deps */
    queue_Append(&parent->dep_children, &dep->parent_list);

    dep->child = child;
    dep->parent = parent;

 error:
    return ret;
}

/**
 * add a dependency to a work node.
 *
 * @param[in] child  node which will be dependent upon completion of parent
 * @param[in] parent node whose completion gates child's execution
 *
 * @pre
 *   - child is in initial state (last op was afs_wq_node_alloc or afs_wq_node_wait)
 *
 * @return operation status
 *    @retval 0 success
 */
int
afs_wq_node_dep_add(struct afs_work_queue_node * child,
		    struct afs_work_queue_node * parent)
{
    int ret = 0;
    struct afs_work_queue_dep_node * dep = NULL;
    struct afs_work_queue_node_multilock ml;
    int held = 0;

    /* self references are bad, mmkay? */
    if (parent == child) {
	ret = AFS_WQ_ERROR;
	goto error;
    }

    ret = _afs_wq_dep_alloc(&dep);
    if (ret) {
	goto error;
    }

    memset(&ml, 0, sizeof(ml));
    ml.nodes[0].node = parent;
    ml.nodes[1].node = child;
    ret = _afs_wq_node_multilock(&ml);
    if (ret) {
	goto error;
    }
    held = 1;

    /* only allow dep modification while in initial state
     * or running state (e.g. do a dep add while inside callback) */
    if ((child->state != AFS_WQ_NODE_STATE_INIT) &&
	(child->state != AFS_WQ_NODE_STATE_RUNNING)) {
	ret = AFS_WQ_ERROR;
	goto error;
    }

    /* link dep node with child and parent work queue node */
    ret = _afs_wq_dep_link_r(dep, parent, child);
    if (ret) {
	goto error;
    }

    /* handle blocking counts */
    switch (parent->state) {
    case AFS_WQ_NODE_STATE_INIT:
    case AFS_WQ_NODE_STATE_SCHEDULED:
    case AFS_WQ_NODE_STATE_RUNNING:
    case AFS_WQ_NODE_STATE_BLOCKED:
	child->block_count++;
	break;

    case AFS_WQ_NODE_STATE_ERROR:
	child->error_count++;
	break;

    default:
	(void)0; /* nop */
    }

 done:
    if (held) {
	MUTEX_EXIT(&child->lock);
	MUTEX_EXIT(&parent->lock);
    }
    return ret;

 error:
    if (dep) {
	_afs_wq_dep_free(dep);
    }
    goto done;
}

/**
 * remove a dependency from a work node.
 *
 * @param[in] child  node which was dependent upon completion of parent
 * @param[in] parent node whose completion gated child's execution
 *
 * @return operation status
 *    @retval 0 success
 */
int
afs_wq_node_dep_del(struct afs_work_queue_node * child,
		    struct afs_work_queue_node * parent)
{
    int code, ret = 0;
    struct afs_work_queue_dep_node * dep, * ndep;
    struct afs_work_queue_node_multilock ml;
    int held = 0;

    memset(&ml, 0, sizeof(ml));
    ml.nodes[0].node = parent;
    ml.nodes[1].node = child;
    code = _afs_wq_node_multilock(&ml);
    if (code) {
	goto error;
    }
    held = 1;

    /* only permit changes while child is in init state
     * or running state (e.g. do a dep del when in callback func) */
    if ((child->state != AFS_WQ_NODE_STATE_INIT) &&
	(child->state != AFS_WQ_NODE_STATE_RUNNING)) {
	ret = AFS_WQ_ERROR;
	goto error;
    }

    /* locate node linking parent and child */
    for (queue_Scan(&parent->dep_children,
		    dep,
		    ndep,
		    afs_work_queue_dep_node)) {
	if ((dep->child == child) &&
	    (dep->parent == parent)) {

	    /* no need to grab an extra ref on dep->child here; the caller
	     * should already have a ref on dep->child */
	    code = _afs_wq_dep_unlink_r(dep);
	    if (code) {
		ret = code;
		goto error;
	    }

	    code = _afs_wq_dep_free(dep);
	    if (code) {
		ret = code;
		goto error;
	    }
	    break;
	}
    }

 error:
    if (held) {
	MUTEX_EXIT(&child->lock);
	MUTEX_EXIT(&parent->lock);
    }
    return ret;
}

/**
 * block a work node from execution.
 *
 * this can be used to allow external events to influence work queue flow.
 *
 * @param[in] node  work queue node to be blocked
 *
 * @return operation status
 *    @retval 0 success
 *
 * @post external block count incremented
 */
int
afs_wq_node_block(struct afs_work_queue_node * node)
{
    int ret = 0;
    int start;

    MUTEX_ENTER(&node->lock);
    ret = _afs_wq_node_state_wait_busy(node);
    if (ret) {
	goto error_sync;
    }

    start = node->block_count++;

    if (!start &&
	(node->qidx == AFS_WQ_NODE_LIST_READY)) {
	/* unblocked->blocked transition, and we're already scheduled */
	ret = _afs_wq_node_list_remove(node,
					    AFS_WQ_NODE_STATE_BUSY);
	if (ret) {
	    goto error_sync;
	}

	ret = _afs_wq_node_list_enqueue(&node->queue->blocked_list,
					     node,
					     AFS_WQ_NODE_STATE_BLOCKED);
    }

 error_sync:
    MUTEX_EXIT(&node->lock);

    return ret;
}

/**
 * unblock a work node for execution.
 *
 * this can be used to allow external events to influence work queue flow.
 *
 * @param[in] node  work queue node to be blocked
 *
 * @return operation status
 *    @retval 0 success
 *
 * @post external block count decremented
 */
int
afs_wq_node_unblock(struct afs_work_queue_node * node)
{
    int ret = 0;
    int end;

    MUTEX_ENTER(&node->lock);
    ret = _afs_wq_node_state_wait_busy(node);
    if (ret) {
	goto error_sync;
    }

    end = --node->block_count;

    if (!end &&
	(node->qidx == AFS_WQ_NODE_LIST_BLOCKED)) {
	/* blocked->unblock transition, and we're ready to be scheduled */
	ret = _afs_wq_node_list_remove(node,
					    AFS_WQ_NODE_STATE_BUSY);
	if (ret) {
	    goto error_sync;
	}

	ret = _afs_wq_node_list_enqueue(&node->queue->ready_list,
					     node,
					     AFS_WQ_NODE_STATE_SCHEDULED);
    }

 error_sync:
    MUTEX_EXIT(&node->lock);

    return ret;
}

/**
 * initialize a afs_wq_add_opts struct with the default options.
 *
 * @param[out] opts  options structure to initialize
 */
void
afs_wq_add_opts_init(struct afs_work_queue_add_opts *opts)
{
    opts->donate = 0;
    opts->block = 1;
    opts->force = 0;
}

/**
 * schedule a work node for execution.
 *
 * @param[in] queue  work queue
 * @param[in] node   work node
 * @param[in] opts   options for adding, or NULL for defaults
 *
 * @return operation status
 *    @retval 0 success
 *    @retval EWOULDBLOCK queue is full and opts specified not to block
 *    @retval EINTR queue was full, we blocked to add, and the queue was
 *                  shutdown while we were blocking
 */
int
afs_wq_add(struct afs_work_queue *queue,
           struct afs_work_queue_node *node,
           struct afs_work_queue_add_opts *opts)
{
    int ret = 0;
    int donate, block, force, hithresh;
    struct afs_work_queue_node_list * list;
    struct afs_work_queue_add_opts l_opts;
    int waited_for_drain = 0;
    afs_wq_work_state_t state;

    if (!opts) {
	afs_wq_add_opts_init(&l_opts);
	opts = &l_opts;
    }

    donate = opts->donate;
    block = opts->block;
    force = opts->force;

 retry:
    MUTEX_ENTER(&node->lock);

    ret = _afs_wq_node_state_wait_busy(node);
    if (ret) {
	goto error;
    }

    if (!node->block_count && !node->error_count) {
	list = &queue->ready_list;
	state = AFS_WQ_NODE_STATE_SCHEDULED;
    } else if (node->error_count) {
	list = &queue->done_list;
	state = AFS_WQ_NODE_STATE_ERROR;
    } else {
	list = &queue->blocked_list;
	state = AFS_WQ_NODE_STATE_BLOCKED;
    }

    ret = 0;

    MUTEX_ENTER(&queue->lock);

    if (queue->shutdown) {
	ret = EINTR;
	MUTEX_EXIT(&queue->lock);
	MUTEX_EXIT(&node->lock);
	goto error;
    }

    hithresh = queue->opts.pend_hithresh;
    if (hithresh > 0 && queue->pend_count >= hithresh) {
	queue->drain = 1;
    }

    if (!force && (state == AFS_WQ_NODE_STATE_SCHEDULED
                   || state == AFS_WQ_NODE_STATE_BLOCKED)) {

	if (queue->drain) {
	    if (block) {
		MUTEX_EXIT(&node->lock);
		CV_WAIT(&queue->pend_cv, &queue->lock);

		if (queue->shutdown) {
		    ret = EINTR;
		} else {
		    MUTEX_EXIT(&queue->lock);

		    waited_for_drain = 1;

		    goto retry;
		}
	    } else {
		ret = EWOULDBLOCK;
	    }
	}
    }

    if (ret == 0) {
	queue->pend_count++;
    }
    if (waited_for_drain) {
	/* signal another thread that may have been waiting for drain */
	CV_SIGNAL(&queue->pend_cv);
    }

    MUTEX_EXIT(&queue->lock);

    if (ret) {
	goto error;
    }

    if (!donate)
	node->refcount++;
    node->queue = queue;

    ret = _afs_wq_node_list_enqueue(list,
					 node,
					 state);
 error:
    return ret;
}

/**
 * de-schedule a work node.
 *
 * @param[in] node  work node
 *
 * @return operation status
 *    @retval 0 success
 */
int
afs_wq_del(struct afs_work_queue_node * node)
{
    /* XXX todo */
    return ENOTSUP;
}

/**
 * execute a node on the queue.
 *
 * @param[in] queue  work queue
 * @param[in] rock   opaque pointer (passed as third arg to callback func)
 *
 * @return operation status
 *    @retval 0 completed a work unit
 */
int
afs_wq_do(struct afs_work_queue * queue,
	  void * rock)
{
    return _afs_wq_do(queue, rock, 1);
}

/**
 * execute a node on the queue, if there is any work to do.
 *
 * @param[in] queue  work queue
 * @param[in] rock   opaque pointer (passed as third arg to callback func)
 *
 * @return operation status
 *    @retval 0 completed a work unit
 *    @retval EWOULDBLOCK there was nothing to do
 */
int
afs_wq_do_nowait(struct afs_work_queue * queue,
		 void * rock)
{
    return _afs_wq_do(queue, rock, 0);
}

/**
 * wait for all pending nodes to finish.
 *
 * @param[in] queue  work queue
 *
 * @return operation status
 *   @retval 0 success
 *
 * @post the specified queue was empty at some point; it may not be empty by
 * the time this function returns, but at some point after the function was
 * called, there were no nodes in the ready queue or blocked queue.
 */
int
afs_wq_wait_all(struct afs_work_queue *queue)
{
    int ret = 0;

    MUTEX_ENTER(&queue->lock);

    while (queue->pend_count > 0 && !queue->shutdown) {
	CV_WAIT(&queue->empty_cv, &queue->lock);
    }

    if (queue->shutdown) {
	/* queue has been shut down, but there may still be some threads
	 * running e.g. in the middle of their callback. ensure they have
	 * stopped before we return. */
	while (queue->running_count > 0) {
	    CV_WAIT(&queue->running_cv, &queue->lock);
	}
	ret = EINTR;
	goto done;
    }

 done:
    MUTEX_EXIT(&queue->lock);

    /* technically this doesn't really guarantee that the work queue is empty
     * after we return, but we do guarantee that it was empty at some point */

    return ret;
}

/**
 * wait for a node to complete; dequeue from done list.
 *
 * @param[in]  node     work queue node
 * @param[out] retcode  return code from work unit
 *
 * @return operation status
 *    @retval 0 sucess
 *
 * @pre ref held on node
 */
int
afs_wq_node_wait(struct afs_work_queue_node * node,
		 int * retcode)
{
    int ret = 0;

    MUTEX_ENTER(&node->lock);
    if (node->state == AFS_WQ_NODE_STATE_INIT) {
	/* not sure what to do in this case */
	goto done_sync;
    }

    while ((node->state != AFS_WQ_NODE_STATE_DONE) &&
	   (node->state != AFS_WQ_NODE_STATE_ERROR)) {
	CV_WAIT(&node->state_cv, &node->lock);
    }
    if (retcode) {
	*retcode = node->retcode;
    }

    if (node->queue == NULL) {
	/* nothing we can do */
	goto done_sync;
    }

    ret = _afs_wq_node_list_remove(node,
					AFS_WQ_NODE_STATE_INIT);

 done_sync:
    MUTEX_EXIT(&node->lock);

    return ret;
}
