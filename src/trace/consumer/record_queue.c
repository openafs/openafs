/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi tracing framework
 * trace consumer library
 * tracepoint record cache
 */

#include <trace/common/trace_impl.h>
#include <trace/consumer/record_queue.h>
#include <trace/consumer/record_queue_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_mutex.h>
#include <osi/osi_condvar.h>

osi_mem_object_cache_t * osi_trace_consumer_record_cache = osi_NULL;
osi_mem_object_cache_t * osi_trace_consumer_record_queue_cache = osi_NULL;

osi_trace_consumer_record_queue_t * osi_trace_consumer_incoming_record_queue = osi_NULL;

/* static prototypes */
osi_static osi_result
osi_trace_consumer_record_queue_alloc_record(osi_TracePoint_record_queue_t **);
osi_static int
osi_trace_consumer_record_queue_cache_ctor(void * buf, void * sdata, int flags);
osi_static void
osi_trace_consumer_record_queue_cache_dtor(void * buf, void * sdata);


osi_static int
osi_trace_consumer_record_queue_cache_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_consumer_record_queue_t * qe;
    osi_condvar_options_t cv_opt;
    osi_mutex_options_t mutex_opt;

    qe = (osi_trace_consumer_record_queue_t *) buf;

    osi_condvar_options_Init(&cv_opt);
    osi_condvar_options_Set(&cv_opt, OSI_CONDVAR_OPTION_TRACE_ALLOWED, 0);
    osi_condvar_Init(&qe->cv, &cv_opt);
    osi_condvar_options_Destroy(&cv_opt);

    osi_mutex_options_Init(&mutex_opt);
    osi_mutex_options_Set(&mutex_opt, OSI_MUTEX_OPTION_TRACE_ALLOWED, 0);
    osi_mutex_Init(&qe->lock, &mutex_opt);
    osi_mutex_options_Destroy(&mutex_opt);

    osi_list_Init(&qe->record_list);

    return 0;
}

osi_static void
osi_trace_consumer_record_queue_cache_dtor(void * buf, void * sdata)
{
    osi_trace_consumer_record_queue_t * qe;

    qe = (osi_trace_consumer_record_queue_t *) buf;
    osi_condvar_Destroy(&qe->cv);
    osi_mutex_Destroy(&qe->lock);
}

osi_result
osi_trace_consumer_record_queue_create(osi_trace_consumer_record_queue_t ** queue_out)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_record_queue_t * queue;

    *queue_out = queue = (osi_trace_consumer_record_queue_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_record_queue_cache);
    if (osi_compiler_expect_false(queue == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    queue->record_list_len = 0;
    queue->waiter_cancel = OSI_FALSE;

 error:
    return res;
}

osi_result
osi_trace_consumer_record_queue_destroy(osi_trace_consumer_record_queue_t * queue)
{
    osi_mem_object_cache_free(osi_trace_consumer_record_queue_cache,
			      queue);
    return OSI_OK;
}

osi_static osi_result
osi_trace_consumer_record_queue_alloc_record(osi_TracePoint_record_queue_t ** record_out)
{
    osi_result res = OSI_OK;
    osi_TracePoint_record_queue_t * record;

    *record_out = record = (osi_TracePoint_record_queue_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_record_cache);
    if (osi_compiler_expect_false(record == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

osi_result
osi_trace_consumer_record_queue_free_record(osi_TracePoint_record_queue_t * record)
{
    osi_mem_object_cache_free(osi_trace_consumer_record_cache,
			      record);
    return OSI_OK;
}

/*
 * enqueue a record onto a queue
 *
 * [IN] queue   -- pointer to queue
 * [IN] record  -- pointer to record
 *
 * preconditions:
 *   queue->lock not held
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_record_queue_enqueue(osi_trace_consumer_record_queue_t * queue,
					osi_TracePoint_record_queue_t * record)
{
    osi_result res = OSI_OK;

    osi_mutex_Lock(&queue->lock);
    osi_list_Append(&queue->record_list,
		    record,
		    osi_TracePoint_record_queue_t,
		    record_list);
    if (++queue->record_list_len == 1) {
	osi_condvar_Signal(&queue->cv);
    }
    osi_mutex_Unlock(&queue->lock);

    return res;
}

/*
 * wait for the queue to become non-empty and then dequeue the first record
 *
 * [IN] queue          -- queue from which to dequeue record
 * [INOUT] record_out  -- address in which to store dequeued record pointer
 *
 * preconditions:
 *   queue->lock not held
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_record_queue_dequeue(osi_trace_consumer_record_queue_t * queue,
					osi_TracePoint_record_queue_t ** record_out)
{
    osi_result res = OSI_OK;
    osi_TracePoint_record_queue_t * record;

    osi_mutex_Lock(&queue->lock);
    while (!queue->record_list_len) {
	osi_condvar_Wait(&queue->cv, &queue->lock);
	if (queue->waiter_cancel == OSI_TRUE) {
	    res = OSI_ERROR_CANCEL_WAITERS;
	    goto error_sync;
	}
    }
    record = osi_list_First(&queue->record_list,
			    osi_TracePoint_record_queue_t,
			    record_list);
    osi_list_Remove(record,
		    osi_TracePoint_record_queue_t,
		    record_list);
    queue->record_list_len--;

 error_sync:
    osi_mutex_Unlock(&queue->lock);

    *record_out = record;

    return res;
}

/*
 * try to dequeue the first record from the queue
 *
 * [IN] queue          -- queue from which to dequeue record
 * [INOUT] record_out  -- address in which to store dequeued record pointer
 *
 * preconditions:
 *   queue->lock not held
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on empty queue
 */
osi_result
osi_trace_consumer_record_queue_dequeue_nosleep(osi_trace_consumer_record_queue_t * queue,
						osi_TracePoint_record_queue_t ** record_out)
{
    osi_result res = OSI_OK;
    osi_TracePoint_record_queue_t * record = osi_NULL;

    osi_mutex_Lock(&queue->lock);
    if (queue->waiter_cancel == OSI_TRUE) {
	res = OSI_ERROR_CANCEL_WAITERS;
    } else if (!queue->record_list_len) {
	res = OSI_FAIL;
    } else {
	record = osi_list_First(&queue->record_list,
				osi_TracePoint_record_queue_t,
				record_list);
	osi_list_Remove(record,
			osi_TracePoint_record_queue_t,
			record_list);
	queue->record_list_len--;
    }
    osi_mutex_Unlock(&queue->lock);

    *record_out = record;

    return res;
}

/*
 * enqueue a new record onto the incoming queue
 *
 * [IN] record  -- pointer to incoming record
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result 
osi_trace_consumer_record_queue_enqueue_new(osi_TracePoint_record * record)
{
    osi_result res = OSI_OK;
    osi_TracePoint_record_queue_t * qe;

    res = osi_trace_consumer_record_queue_alloc_record(&qe);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    osi_mem_copy(&qe->record, record, sizeof(*record));
    osi_mem_zero(&qe->state, sizeof(qe->state));
    qe->state.state = OSI_TRACE_CONSUMER_RECORD_STATE_INCOMING;
    qe->state.next_state = OSI_TRACE_CONSUMER_RECORD_STATE_MAX_ID;

    res = osi_trace_consumer_record_queue_enqueue(osi_trace_consumer_incoming_record_queue, qe);

 error:
    return res;
}

/*
 * flag that all waiter threads/tasklets should cancel themselves
 *
 * [IN] queue
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_record_queue_waiter_cancel(osi_trace_consumer_record_queue_t * queue)
{
    osi_mutex_Lock(&queue->lock);
    queue->waiter_cancel = OSI_TRUE;
    osi_mutex_Unlock(&queue->lock);

    return OSI_OK;
}

/*
 * iterate through a record queue calling a function pointer for each element
 *
 * [IN] queue  -- pointer to queue object
 * [IN] func   -- worker function pointer
 * [IN] sdata  -- opaque data to pass to worker function
 *
 * preconditions:
 *   queue->lock not held
 *
 * returns:
 *   OSI_OK if all worker invocations evaluate to OSI_RESULT_OK()
 *   OSI_FAIL if any worker invocation evaluates to OSI_RESULT_FAIL()
 */
osi_result
osi_trace_consumer_record_queue_foreach(osi_trace_consumer_record_queue_t * queue,
					osi_trace_consumer_record_queue_worker_t * func,
					void * sdata)
{
    osi_result res, code = OSI_OK;
    osi_TracePoint_record_queue_t * rec, * nrec;

    osi_mutex_Lock(&queue->lock);
    for (osi_list_Scan(&queue->record_list,
		       rec, nrec,
		       osi_TracePoint_record_queue_t,
		       record_list)) {
	res = (*func)(queue, rec, sdata);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = OSI_FAIL;
	}
    }
    osi_mutex_Unlock(&queue->lock);

    return code;
}

/*
 * do bulk state transitions on a record queue:
 *
 * first, iterate through the entire queue running a function pointer
 * for each record where next_state is updated, move to a temporary queue
 * drop the lock on the main queue
 * finally, move everything from the temp queue to the appropriate dest queue
 *
 * needless to say, lock ordering is very important in order to avoid deadlock
 *
 * [IN] queue  -- pointer to queue object
 * [IN] func   -- worker function pointer
 * [IN] sdata  -- opaque data to pass to worker function
 *
 * preconditions:
 *   queue->lock not held
 *
 * returns:
 *   OSI_OK if all worker invocations evaluate to OSI_RESULT_OK()
 *   OSI_FAIL if any worker invocation evaluates to OSI_RESULT_FAIL()
 */
osi_extern osi_result
osi_trace_consumer_record_queue_bulk_trans(osi_trace_consumer_record_queue_t * queue,
					   osi_trace_consumer_record_queue_worker_t * func,
					   void * sdata)
{
    osi_result res, code;
    osi_trace_consumer_record_queue_t * temp_queue;
    osi_TracePoint_record_queue_t * rec, * nrec;
    osi_uint32 transferred = 0;

    code = osi_trace_consumer_record_queue_create(&temp_queue);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    /* for deadlock avoidance, transition records which are ready for
     * state transition to a temporary queue */
    osi_mutex_Lock(&queue->lock);
    osi_mutex_Lock(&temp_queue->lock);
    for (osi_list_Scan(&queue->record_list,
		       rec, nrec,
		       osi_TracePoint_record_queue_t,
		       record_list)) {
	rec->state.next_state = OSI_TRACE_CONSUMER_RECORD_STATE_MAX_ID;
	res = (*func)(queue, rec, sdata);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = OSI_FAIL;
	} else if (rec->state.next_state != 
		   OSI_TRACE_CONSUMER_RECORD_STATE_MAX_ID) {
	    osi_list_MoveAppend(&temp_queue->record_list,
				rec,
				osi_TracePoint_record_queue_t,
				record_list);
	    transferred++;
	}
    }
    queue->record_list_len -= transferred;
    osi_mutex_Unlock(&queue->lock);

    /* now perform state transitions */
    for (osi_list_Scan(&temp_queue->record_list,
		       rec, nrec,
		       osi_TracePoint_record_queue_t,
		       record_list)) {
	osi_list_Remove(rec,
			osi_TracePoint_record_queue_t,
			record_list);
	res = osi_trace_consumer_record_fsa_transition(rec);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }
    osi_mutex_Unlock(&temp_queue->lock);

    res = osi_trace_consumer_record_queue_destroy(temp_queue);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

 error:
    return code;
}

osi_result 
osi_trace_consumer_record_queue_PkgInit(void)
{
    osi_result res = OSI_OK;
    osi_mem_object_cache_options_t cache_opt;

    osi_mem_object_cache_options_Init(&cache_opt);
    osi_mem_object_cache_options_Set(&cache_opt, 
				     OSI_MEM_OBJECT_CACHE_OPTION_TRACE_ALLOWED, 
				     0);

    osi_trace_consumer_record_queue_cache =
	osi_mem_object_cache_create("osi_trace_consumer_record_queue",
				    sizeof(osi_trace_consumer_record_queue_t),
				    0,
				    osi_NULL,
				    &osi_trace_consumer_record_queue_cache_ctor,
				    &osi_trace_consumer_record_queue_cache_dtor,
				    osi_NULL,
				    &cache_opt);
    if (osi_trace_consumer_record_queue_cache == osi_NULL) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_trace_consumer_record_cache =
	osi_mem_object_cache_create("osi_trace_consumer_record",
				    sizeof(osi_TracePoint_record_queue_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    &cache_opt);
    if (osi_trace_consumer_record_cache == osi_NULL) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    res = osi_trace_consumer_record_queue_create(&osi_trace_consumer_incoming_record_queue);

 error:
    osi_mem_object_cache_options_Destroy(&cache_opt);
    return res;
}

osi_result 
osi_trace_consumer_record_queue_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    osi_trace_consumer_record_queue_destroy(osi_trace_consumer_incoming_record_queue);

    osi_mem_object_cache_destroy(osi_trace_consumer_record_cache);
    osi_mem_object_cache_destroy(osi_trace_consumer_record_queue_cache);

 error:
    return res;
}
