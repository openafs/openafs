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
#include <trace/USERSPACE/gen_rgy.h>
#include <trace/common/options.h>
#include <trace/consumer/module.h>
#include <trace/consumer/cache/generator.h>
#include <trace/consumer/cache/probe_value.h>

/*
 * osi tracing framework
 * trace consumer library
 * cache subsystem
 * gen rgy cache coherency 
 */

typedef enum {
    OSI_TRACE_CONSUMER_CACHE_GEN_UP,
    OSI_TRACE_CONSUMER_CACHE_GEN_DOWN
} osi_trace_consumer_gen_rgy_update_op_t;

typedef struct {
    osi_list_element_volatile update_list;
    osi_trace_gen_id_t osi_volatile gen_id;
    osi_trace_consumer_gen_rgy_update_op_t op;
} osi_trace_consumer_gen_rgy_update_t;

osi_mem_object_cache_t * osi_trace_consumer_gen_rgy_update_cache = osi_NULL;

struct {
    osi_list_head_volatile update_list;
    osi_uint32 osi_volatile update_list_len;
    osi_condvar_t cv;
    osi_mutex_t lock;
} osi_trace_consumer_gen_rgy_update;


/*
 * enqueue gen up event
 *
 * [IN] gen_id  -- generator id
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_consumer_cache_notify_gen_up(osi_trace_gen_id_t gen_id)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_gen_rgy_update_t * node;

    node = osi_mem_object_cache_alloc(osi_trace_consumer_gen_rgy_update_cache);
    if (osi_compiler_expect_false(node == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    node->gen_id = gen_id;
    node->op = OSI_TRACE_CONSUMER_CACHE_GEN_UP;

    osi_mutex_Lock(&osi_trace_consumer_gen_rgy_update.lock);
    osi_list_Append(&osi_trace_consumer_gen_rgy_update.update_list,
		    node,
		    osi_trace_consumer_gen_rgy_update_t,
		    update_list);
    if (!osi_trace_consumer_gen_rgy_update.update_list_len++) {
	osi_condvar_Signal(&osi_trace_consumer_gen_rgy_update.cv);
    }
    osi_mutex_Unlock(&osi_trace_consumer_gen_rgy_update.lock);

 error:
    return res;
}

/*
 * enqueue gen down event
 *
 * [IN] gen_id  -- generator id
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_consumer_cache_notify_gen_down(osi_trace_gen_id_t gen_id)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_gen_rgy_update_t * node;

    node = osi_mem_object_cache_alloc(osi_trace_consumer_gen_rgy_update_cache);
    if (osi_compiler_expect_false(node == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    node->gen_id = gen_id;
    node->op = OSI_TRACE_CONSUMER_CACHE_GEN_DOWN;

    osi_mutex_Lock(&osi_trace_consumer_gen_rgy_update.lock);
    osi_list_Append(&osi_trace_consumer_gen_rgy_update.update_list,
		    node,
		    osi_trace_consumer_gen_rgy_update_t,
		    update_list);
    if (!osi_trace_consumer_gen_rgy_update.update_list_len++) {
	osi_condvar_Signal(&osi_trace_consumer_gen_rgy_update.cv);
    }
    osi_mutex_Unlock(&osi_trace_consumer_gen_rgy_update.lock);

 error:
    return res;
}

/*
 * helper function to handle recently started gen processes
 *
 * [IN] gen_id  -- generator id
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_consumer_gen_rgy_handle_up(osi_trace_gen_id_t gen_id)
{
    osi_result res, code = OSI_OK;
    osi_trace_generator_info_t info;
    osi_trace_consumer_gen_cache_t * gen;
    int have_ref = 0;

    /* first, get a ref on this gen to guarantee cache coherency */
    res = osi_trace_gen_rgy_get(gen_id);
    if (OSI_RESULT_OK_LIKELY(res)) {
	have_ref = 1;
    } else {
	(osi_Msg "%s: WARNING: failed to get ref on gen %d.  "
	 "cache coherency cannot be assured.\n", __osi_func__, gen_id);
    }

    /* now, query trace module metadata */
    code = osi_trace_module_info(gen_id, &info);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto cleanup;
    }

    code = osi_trace_consumer_gen_cache_alloc(&info, &gen);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto cleanup;
    }

    code = osi_trace_consumer_gen_cache_register(gen);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto cleanup;
    }

    (void)osi_trace_consumer_gen_cache_put(gen);

 error:
    return code;

 cleanup:
    if (have_ref) {
	res = osi_trace_gen_rgy_put(gen_id);
	if (OSI_RESULT_FAIL(res)) {
	    (osi_Msg "%s: WARNING: failed to put ref on gen %d during rollback.\n",
	     __osi_func__, gen_id);
	}
    }
    if (gen) {
	osi_trace_consumer_gen_cache_put(gen);
    }
    goto error;
}

/*
 * helper function to handle recently dead gen processes
 *
 * [IN] gen_id  -- generator id
 *
 * returns:
 *   OSI_OK on success
 */
osi_static osi_result
osi_trace_consumer_gen_rgy_handle_down(osi_trace_gen_id_t gen_id)
{
    osi_result code = OSI_OK;
    osi_trace_generator_info_t info;
    osi_trace_consumer_gen_cache_t * gen;
    int have_ref = 0;

    code = osi_trace_consumer_gen_cache_lookup(gen_id, &gen);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_consumer_gen_cache_unregister(gen);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto cleanup;
    }

    code = osi_trace_consumer_gen_cache_invalidate(gen);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto cleanup;
    }

 cleanup:
    (void)osi_trace_consumer_gen_cache_put(gen);

 error:
    (void)osi_trace_gen_rgy_put(gen_id);
    return code;
}

/*
 * thread which handles initial population for
 * new generators, and invalidation of old generators
 */
void *
osi_trace_consumer_gen_rgy_coherency_thread(void * arg)
{
    osi_result res;
    osi_trace_consumer_gen_rgy_update_t * node;

    while (1) {
	osi_mutex_Lock(&osi_trace_consumer_gen_rgy_update.lock);
	while (!osi_trace_consumer_gen_rgy_update.update_list_len) {
	    osi_condvar_Wait(&osi_trace_consumer_gen_rgy_update.cv,
			     &osi_trace_consumer_gen_rgy_update.lock);
	}

	node = osi_list_First(&osi_trace_consumer_gen_rgy_update.update_list,
			      osi_trace_consumer_gen_rgy_update_t,
			      update_list);
	osi_list_Remove(node,
			osi_trace_consumer_gen_rgy_update_t,
			update_list);
	osi_trace_consumer_gen_rgy_update.update_list_len--;
	osi_mutex_Unlock(&osi_trace_consumer_gen_rgy_update.lock);

	switch (node->op) {
	case OSI_TRACE_CONSUMER_CACHE_GEN_UP:
	    (void)osi_trace_consumer_gen_rgy_handle_up(node->gen_id);
	    break;
	case OSI_TRACE_CONSUMER_CACHE_GEN_DOWN:
	    (void)osi_trace_consumer_gen_rgy_handle_down(node->gen_id);
	    break;
	}

	osi_mem_object_cache_free(osi_trace_consumer_gen_rgy_update_cache,
				  node);
    }

 error:
    return NULL;
}

/*
 * start i2n background worker threads
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on thread create failure
 */
osi_static osi_result
osi_trace_consumer_gen_cache_thread_start(void)
{
    osi_result res = OSI_OK;
    osi_thread_p t;
    osi_thread_options_t opt;

    osi_thread_options_Init(&opt);
    osi_thread_options_Set(&opt, OSI_THREAD_OPTION_DETACHED, 1);
    osi_thread_options_Set(&opt, OSI_THREAD_OPTION_TRACE_ALLOWED, 0);

    res = osi_thread_createU(&t,
			     &osi_trace_consumer_gen_rgy_coherency_thread,
			     osi_NULL,
			     &opt);

 error:
    osi_thread_options_Destroy(&opt);
    return res;
}

osi_result
osi_trace_consumer_gen_cache_coherency_PkgInit(void)
{
    osi_result code = OSI_OK;
    osi_uint32 i;

    osi_mutex_Init(&osi_trace_consumer_gen_rgy_update.lock,
		   &osi_trace_common_options.mutex_opts);
    osi_condvar_Init(&osi_trace_consumer_gen_rgy_update.cv,
		     &osi_trace_common_options.condvar_opts);

    osi_list_Init(&osi_trace_consumer_gen_rgy_update.update_list);
    osi_trace_consumer_gen_rgy_update.update_list_len = 0;

    osi_trace_consumer_gen_rgy_update_cache = 
	osi_mem_object_cache_create("osi_trace_consumer_gen_rgy_update",
				    sizeof(osi_trace_consumer_gen_rgy_update_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_trace_consumer_gen_rgy_update_cache == osi_NULL) {
	code = OSI_ERROR_NOMEM;
	goto error;
    }

    code = osi_trace_consumer_gen_cache_thread_start();

 error:
    return code;
}

osi_result
osi_trace_consumer_gen_cache_coherency_PkgShutdown(void)
{
    osi_result code = OSI_OK;
    osi_uint32 i;

    osi_mutex_Destroy(&osi_trace_consumer_gen_rgy_update.lock);
    osi_condvar_Destroy(&osi_trace_consumer_gen_rgy_update.cv);
    osi_mem_object_cache_destroy(osi_trace_consumer_gen_rgy_update_cache);

 error:
    return code;
}
