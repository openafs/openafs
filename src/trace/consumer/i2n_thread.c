/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_thread.h>
#include <osi/osi_mem.h>
#include <osi/osi_condvar.h>
#include <osi/osi_mutex.h>
#include <osi/osi_object_cache.h>
#include <trace/directory.h>
#include <trace/gen_rgy.h>
#include <trace/common/options.h>
#include <trace/consumer/i2n.h>
#include <trace/consumer/i2n_thread.h>
#include <trace/consumer/record_queue.h>
#include <trace/consumer/record_queue_impl.h>

/*
 * osi tracing framework
 * trace consumer library
 * i2n resolution queue 
 * control threads
 */

typedef struct {
    osi_list_element_volatile update_list;
    osi_trace_gen_id_t osi_volatile gen_id;
    osi_trace_probe_id_t osi_volatile probe_id;
} osi_trace_consumer_i2n_update_t;

osi_mem_object_cache_t * osi_trace_consumer_i2n_update_cache = osi_NULL;

struct {
    osi_list_head_volatile update_list;
    osi_uint32 osi_volatile update_list_len;
    osi_condvar_t cv;
    osi_mutex_t lock;
} osi_trace_consumer_i2n_update;


typedef struct {
    osi_trace_consumer_i2n_update_t * update_node;
} osi_trace_consumer_i2n_update_work_t;

#define OSI_TRACE_CONSUMER_I2N_RECORD_QUEUE_HASH_BUCKETS 64
#define OSI_TRACE_CONSUMER_I2N_RECORD_QUEUE_HASH_MASK \
    (OSI_TRACE_CONSUMER_I2N_RECORD_QUEUE_HASH_BUCKETS-1)

osi_trace_consumer_record_queue_t * 
osi_trace_consumer_i2n_record_queue[OSI_TRACE_CONSUMER_I2N_RECORD_QUEUE_HASH_BUCKETS] =
    {
	osi_NULL, osi_NULL, osi_NULL, osi_NULL, /* 0--15 */
	osi_NULL, osi_NULL, osi_NULL, osi_NULL,
	osi_NULL, osi_NULL, osi_NULL, osi_NULL, 
	osi_NULL, osi_NULL, osi_NULL, osi_NULL,

	osi_NULL, osi_NULL, osi_NULL, osi_NULL, /* 16--31 */
	osi_NULL, osi_NULL, osi_NULL, osi_NULL,
	osi_NULL, osi_NULL, osi_NULL, osi_NULL, 
	osi_NULL, osi_NULL, osi_NULL, osi_NULL,

	osi_NULL, osi_NULL, osi_NULL, osi_NULL, /* 32--47 */
	osi_NULL, osi_NULL, osi_NULL, osi_NULL,
	osi_NULL, osi_NULL, osi_NULL, osi_NULL, 
	osi_NULL, osi_NULL, osi_NULL, osi_NULL,

	osi_NULL, osi_NULL, osi_NULL, osi_NULL, /* 48--63 */
	osi_NULL, osi_NULL, osi_NULL, osi_NULL,
	osi_NULL, osi_NULL, osi_NULL, osi_NULL, 
	osi_NULL, osi_NULL, osi_NULL, osi_NULL
    };

/* static prototypes */
osi_static osi_result
osi_trace_consumer_i2n_update_worker(osi_trace_consumer_record_queue_t * queue,
				     osi_TracePoint_record_queue_t * record,
				     void * sdata);

/*
 * (gen_id,probe_id)->hash computation function
 *
 * hash function is specifically designed to provide a 6-bit hash,
 * thus yielding the desired 64 hash buckets.
 */
osi_static osi_inline osi_uint32
osi_trace_consumer_i2n_record_queue_hash(osi_trace_gen_id_t gen_id,
					 osi_trace_probe_id_t probe_id)
{
    osi_uint32 hash;
    hash = ((osi_uint32)gen_id) << 24;
    hash ^= (osi_uint32) probe_id;
    hash ^= (hash >> 6);
    hash ^= (hash >> 12);
    hash ^= (hash >> 24);
    hash &= OSI_TRACE_CONSUMER_I2N_RECORD_QUEUE_HASH_MASK;
    return hash;
}

/*
 * thread which handles
 * i2n resolution for incoming trace records
 */
void *
osi_trace_consumer_i2n_handler_thread(void * arg)
{
    osi_result res;
    osi_TracePoint_record_queue_t * rq;
    char name_buf[OSI_TRACE_MAX_PROBE_NAME_LEN];
    osi_uint32 hash;

    while (1) {
	res = osi_trace_consumer_record_queue_dequeue(osi_trace_consumer_incoming_record_queue,
						      &rq);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}

	res = osi_trace_consumer_i2n_lookup(rq->record.gen_id,
					    rq->record.probe,
					    name_buf,
					    sizeof(name_buf));
	if (OSI_RESULT_OK(res)) {
	    /* deal with it immediately */
	} else if (res == OSI_ERROR_REQUEST_QUEUED) {
	    /* enqueue it until I2N resolution is completed */
	    hash = osi_trace_consumer_i2n_record_queue_hash(rq->record.gen_id,
							    rq->record.probe);
	    osi_trace_consumer_record_queue_enqueue(osi_trace_consumer_i2n_record_queue[hash],
						    rq);
	} else {
	    /* throw it away; we probably have a bogus record */
	    osi_trace_consumer_record_queue_free_record(rq);
	}
    }

 error:
    return NULL;
}

/*
 * thread which handles
 * state transition for records previously blocked on i2n resolution
 */
void *
osi_trace_consumer_i2n_unblock_thread(void * arg)
{
    osi_result res;
    osi_trace_consumer_i2n_update_work_t work;
    osi_uint32 hash;
    osi_trace_consumer_record_queue_t * queue;

    osi_mutex_Lock(&osi_trace_consumer_i2n_update.lock);

    while (1) {
	while (!osi_trace_consumer_i2n_update.update_list_len) {
	    osi_condvar_Wait(&osi_trace_consumer_i2n_update.cv,
			     &osi_trace_consumer_i2n_update.lock);
	}

	while (osi_trace_consumer_i2n_update.update_list_len) {
	    work.update_node = osi_list_First(&osi_trace_consumer_i2n_update.update_list,
					      osi_trace_consumer_i2n_update_t,
					      update_list);
	    osi_list_Remove(work.update_node,
			    osi_trace_consumer_i2n_update_t,
			    update_list);
	    osi_trace_consumer_i2n_update.update_list_len--;

	    /* do actual unblocks without the update control lock */
	    osi_mutex_Unlock(&osi_trace_consumer_i2n_update.lock);

	    hash = osi_trace_consumer_i2n_record_queue_hash(work.update_node->gen_id,
							    work.update_node->probe_id);
	    queue = osi_trace_consumer_i2n_record_queue[hash];
	    res = osi_trace_consumer_record_queue_bulk_trans(queue,
							     &osi_trace_consumer_i2n_update_worker,
							     &work);
	    
	    osi_mutex_Lock(&osi_trace_consumer_i2n_update.lock);
	}
    }

 error:
    osi_mutex_Unlock(&osi_trace_consumer_i2n_update.lock);
    return NULL;
}

/*
 * iteration worker function for unblock thread
 * updates next_state field for records which are no longer blocked on i2n
 *
 * [IN] queue   -- queue object
 * [IN] record  -- record object
 * [IN] sdata   -- opaque data (osi_trace_consumer_i2n_update_work_t *)
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_consumer_i2n_update_worker(osi_trace_consumer_record_queue_t * queue,
				     osi_TracePoint_record_queue_t * record,
				     void * sdata)
{
    osi_trace_consumer_i2n_update_work_t * work;

    work = (osi_trace_consumer_i2n_update_work_t *) sdata;

    if ((record->record.gen_id == work->update_node->gen_id) &&
	(record->record.probe == work->update_node->probe_id)) {
	/* match */
	record->state.next_state = OSI_TRACE_CONSUMER_RECORD_STATE_PROBE_CACHE;
    }

    return OSI_OK;
}

/*
 * start i2n background worker threads
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on thread create failure
 */
osi_static osi_result
osi_trace_consumer_i2n_thread_start(void)
{
    osi_result res = OSI_OK;
    osi_thread_p t;
    osi_thread_options_t opt;

    osi_thread_options_Init(&opt);
    osi_thread_options_Set(&opt, OSI_THREAD_OPTION_DETACHED, 1);
    osi_thread_options_Set(&opt, OSI_THREAD_OPTION_TRACE_ALLOWED, 0);

    res = osi_thread_createU(&t,
			     &osi_trace_consumer_i2n_handler_thread,
			     osi_NULL,
			     &opt);

    res = osi_thread_createU(&t,
			     &osi_trace_consumer_i2n_unblock_thread,
			     osi_NULL,
			     &opt);

 error:
    osi_thread_options_Destroy(&opt);
    return res;
}

/*
 * unblock trace records which are pending i2n translation
 *
 * [IN] gen_id
 * [IN] probe_id
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_extern osi_result 
osi_trace_consumer_i2n_thread_unblock(osi_trace_gen_id_t gen_id,
				      osi_trace_probe_id_t probe_id)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_i2n_update_t * node;

    node = (osi_trace_consumer_i2n_update_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_i2n_update_cache);
    if (osi_compiler_expect_false(node == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    node->gen_id = gen_id;
    node->probe_id = probe_id;

    osi_mutex_Lock(&osi_trace_consumer_i2n_update.lock);
    osi_list_Append(&osi_trace_consumer_i2n_update.update_list,
		    node,
		    osi_trace_consumer_i2n_update_t,
		    update_list);
    if (!osi_trace_consumer_i2n_update.update_list_len++) {
	osi_condvar_Signal(&osi_trace_consumer_i2n_update.cv);
    }
    osi_mutex_Unlock(&osi_trace_consumer_i2n_update.lock);

 error:
    return res;
}

osi_result
osi_trace_consumer_i2n_thread_PkgInit(void)
{
    osi_result res, code = OSI_OK;
    osi_uint32 i;

    osi_mutex_Init(&osi_trace_consumer_i2n_update.lock,
		   &osi_trace_common_options.mutex_opts);
    osi_condvar_Init(&osi_trace_consumer_i2n_update.cv,
		     &osi_trace_common_options.condvar_opts);

    osi_list_Init(&osi_trace_consumer_i2n_update.update_list);
    osi_trace_consumer_i2n_update.update_list_len = 0;

    osi_trace_consumer_i2n_update_cache = 
	osi_mem_object_cache_create("osi_trace_consumer_i2n_update",
				    sizeof(osi_trace_consumer_i2n_update_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_trace_consumer_i2n_update_cache == osi_NULL) {
	code = OSI_FAIL;
	goto error;
    }

    for (i = 0; i < OSI_TRACE_CONSUMER_I2N_RECORD_QUEUE_HASH_BUCKETS; i++) {
	res = osi_trace_consumer_record_queue_create(&osi_trace_consumer_i2n_record_queue[i]);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }

    code = osi_trace_consumer_i2n_thread_start();

 error:
    return code;
}

osi_result
osi_trace_consumer_i2n_thread_PkgShutdown(void)
{
    osi_result res, code = OSI_OK;
    osi_uint32 i;

    for (i = 0; i < OSI_TRACE_CONSUMER_I2N_RECORD_QUEUE_HASH_BUCKETS; i++) {
	res = osi_trace_consumer_record_queue_destroy(osi_trace_consumer_i2n_record_queue[i]);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }

    osi_mutex_Destroy(&osi_trace_consumer_i2n_update.lock);
    osi_condvar_Destroy(&osi_trace_consumer_i2n_update.cv);
    osi_mem_object_cache_destroy(osi_trace_consumer_i2n_update_cache);

    return code;
}
