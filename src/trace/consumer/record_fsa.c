/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi tracing framework
 * trace consumer library
 * tracepoint record queueing mechanism
 * record finite state automata
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_rwlock.h>
#include <trace/consumer/record_fsa.h>
#include <trace/consumer/record_queue.h>
#include <trace/consumer/record_queue_impl.h>

/*
 * the following enumeration defines the valid FSA states
 * for a tracepoint record
 */

struct {
    osi_volatile osi_trace_consumer_record_fsa_registration_t 
    registration[OSI_TRACE_CONSUMER_RECORD_STATE_MAX_ID];
    osi_rwlock_t lock;
} osi_trace_consumer_record_fsa_config;

/*
 * transition a trace record from one state to another
 *
 * [IN] record
 * [IN] next_state  -- state to which we are transitioning
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL if no enqueue function pointer is defined for next_state
 *   ? for common case, return code is determined by enqueue function
 */
osi_result
osi_trace_consumer_record_fsa_transition(osi_TracePoint_record_queue_t * record)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_record_fsa_dequeue_func_t * dequeue_fp;
    osi_trace_consumer_record_fsa_enqueue_func_t * enqueue_fp;

    if (osi_compiler_expect_false(record->state.next_state >= 
				  OSI_TRACE_CONSUMER_RECORD_STATE_MAX_ID)) {
	res = OSI_FAIL;
	goto error;
    }

    osi_rwlock_RdLock(&osi_trace_consumer_record_fsa_config.lock);
    if (osi_compiler_expect_true(record->state.state < 
				 OSI_TRACE_CONSUMER_RECORD_STATE_MAX_ID)) {
	dequeue_fp = 
	    osi_trace_consumer_record_fsa_config.registration[record->state.state].dequeue_fp;
    } else {
	dequeue_fp = osi_NULL;
    }
    enqueue_fp = 
	osi_trace_consumer_record_fsa_config.registration[record->state.next_state].enqueue_fp;

    if (dequeue_fp != osi_NULL) {
	res = (*dequeue_fp)(record);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error_sync;
	}
    }

    if (osi_compiler_expect_true(enqueue_fp != osi_NULL)) {
	res = (*enqueue_fp)(record);
    } else {
	/* XXX if this ever happens, then somebody is seriously misusing
	 *     the record fsa */
	res = OSI_FAIL;
    }

 error_sync:
    osi_rwlock_Unlock(&osi_trace_consumer_record_fsa_config.lock);

 error:
    return res;
}

/*
 * register handler functions for a specific record fsa state
 *
 * [IN] state       -- state
 * [IN] dequeue_fp  -- record dequeue handler function pointer
 * [IN] enqueue_fp  -- record enqueue handler function pointer
 * [IN] free_fp     -- record free handler function pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL for invalid state
 */
osi_result
osi_trace_consumer_record_fsa_register(osi_trace_consumer_record_fsa_state_type_t state,
				       osi_trace_consumer_record_fsa_dequeue_func_t * dequeue_fp,
				       osi_trace_consumer_record_fsa_enqueue_func_t * enqueue_fp,
				       osi_trace_consumer_record_fsa_free_func_t * free_fp)
{
    osi_result res = OSI_OK;

    if (state >= OSI_TRACE_CONSUMER_RECORD_STATE_MAX_ID) {
	res = OSI_FAIL;
	goto error;
    }

    osi_rwlock_WrLock(&osi_trace_consumer_record_fsa_config.lock);
    osi_trace_consumer_record_fsa_config.registration[state].dequeue_fp = 
	dequeue_fp;
    osi_trace_consumer_record_fsa_config.registration[state].enqueue_fp = 
	enqueue_fp;
    osi_trace_consumer_record_fsa_config.registration[state].free_fp = 
	free_fp;
    osi_rwlock_Unlock(&osi_trace_consumer_record_fsa_config.lock);

 error:
    return res;
}

osi_result
osi_trace_consumer_record_fsa_PkgInit(void)
{
    osi_rwlock_options_t rw_opt;

    osi_mem_zero(&osi_trace_consumer_record_fsa_config, 
		 sizeof(osi_trace_consumer_record_fsa_config));

    osi_rwlock_options_Init(&rw_opt);
    osi_rwlock_options_Set(&rw_opt, OSI_RWLOCK_OPTION_TRACE_ALLOWED, 0);
    osi_rwlock_Init(&osi_trace_consumer_record_fsa_config.lock, &rw_opt);
    osi_rwlock_options_Destroy(&rw_opt);

    return OSI_OK;
}

osi_result
osi_trace_consumer_record_fsa_PkgShutdown(void)
{
    osi_rwlock_Destroy(&osi_trace_consumer_record_fsa_config.lock);
    return OSI_OK;
}
