/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_list.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_mutex.h>
#include <osi/osi_condvar.h>
#include <trace/console/trap.h>
#include <trace/console/trap_queue.h>
#include <trace/console/trap_types.h>

/*
 * osi tracing framework
 * trace console
 * types for trap support
 */

struct osi_trace_console_trap_queue {
    osi_list_head_volatile trap_list;
    osi_uint32 osi_volatile trap_list_len;
    osi_condvar_t cv;
    osi_mutex_t lock;
};

struct osi_trace_console_trap_queue osi_trace_console_trap_queue;

osi_mem_object_cache_t * osi_trace_console_trap_queue_cache = osi_NULL;

osi_static osi_result
osi_trace_console_trap_queue_enqueue(osi_trace_console_trap_t * trap_in,
				     void * sdata);


/*
 * enable the trap queue
 *
 * returns:
 *   see osi_trace_console_trap_handler_register()
 */
osi_result
osi_trace_console_trap_queue_enable(void)
{
    return osi_trace_console_trap_handler_register(&osi_trace_console_trap_queue_enqueue,
						   osi_NULL);
}

/*
 * disable the trap queue
 *
 * returns:
 *   see osi_trace_console_trap_handler_unregister()
 */
osi_result
osi_trace_console_trap_queue_disable(void)
{
    return osi_trace_console_trap_handler_unregister(&osi_trace_console_trap_queue_enqueue);
}

/*
 * private interface to enqueue a new trap
 * matches osi_trace_console_trap_handler_t typedef
 *
 * [IN] trap_in
 * [IN] sdata
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_console_trap_queue_enqueue(osi_trace_console_trap_t * trap_in,
				     void * sdata)
{
    osi_result code = OSI_OK;
    osi_trace_console_trap_t * trap;

    trap = (osi_trace_console_trap_t *)
	osi_mem_object_cache_alloc(osi_trace_console_trap_queue_cache);
    if (osi_compiler_expect_false(trap == osi_NULL)) {
	code = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_mem_copy(trap, trap_in, sizeof(osi_trace_console_trap_t));

    osi_mutex_Lock(&osi_trace_console_trap_queue.lock);
    osi_list_Append(&osi_trace_console_trap_queue.trap_list,
		    trap,
		    osi_trace_console_trap_t,
		    trap_list);
    if (++osi_trace_console_trap_queue.trap_list_len) {
	osi_condvar_Signal(&osi_trace_console_trap_queue.cv);
    }
    osi_mutex_Unlock(&osi_trace_console_trap_queue.lock);

 error:
    return code;
}

/*
 * get a trap off the queue
 *
 * [OUT] trap_out  -- pointer to trap data
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_console_trap_queue_get(osi_trace_console_trap_t ** trap_out)
{
    osi_trace_console_trap_t * trap;

    osi_mutex_Lock(&osi_trace_console_trap_queue.lock);
    while (!osi_trace_console_trap_queue.trap_list_len) {
	osi_condvar_Wait(&osi_trace_console_trap_queue.cv,
			 &osi_trace_console_trap_queue.lock);
    }
    *trap_out = trap = osi_list_First(&osi_trace_console_trap_queue.trap_list,
				      osi_trace_console_trap_t,
				      trap_list);
    osi_list_Remove(trap,
		    osi_trace_console_trap_t,
		    trap_list);
    osi_trace_console_trap_queue.trap_list_len--;
    osi_mutex_Unlock(&osi_trace_console_trap_queue.lock);

    return OSI_OK;
}

osi_result
osi_trace_console_trap_queue_put(osi_trace_console_trap_t * trap)
{
    if (trap->trap_payload.osi_Trace_rpc_trap_data_len) {
	osi_mem_free(trap->trap_payload.osi_Trace_rpc_trap_data_val,
		     trap->trap_payload.osi_Trace_rpc_trap_data_len);
    }
    osi_mem_object_cache_free(osi_trace_console_trap_queue_cache, trap);
    return OSI_OK;
}

osi_result
osi_trace_console_trap_queue_PkgInit(void)
{
    osi_result res = OSI_OK;

    osi_mutex_Init(&osi_trace_console_trap_queue.lock,
		   osi_trace_impl_mutex_opts());
    osi_condvar_Init(&osi_trace_console_trap_queue.cv,
		     osi_trace_impl_condvar_opts());
    osi_list_Init(&osi_trace_console_trap_queue.trap_list);

    osi_trace_console_trap_queue_cache =
	osi_mem_object_cache_create("osi_trace_console_trap_queue",
				    sizeof(osi_trace_console_trap_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_trace_console_trap_queue_cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

osi_result
osi_trace_console_trap_queue_PkgShutdown(void)
{
    osi_result res = OSI_OK;
    osi_trace_console_trap_t * node, * nn;

    osi_mutex_Lock(&osi_trace_console_trap_queue.lock);
    for (osi_list_Scan(&osi_trace_console_trap_queue.trap_list,
		       node, nn,
		       osi_trace_console_trap_t,
		       trap_list)) {
	osi_list_Remove(node,
			osi_trace_console_trap_t,
			trap_list);
	(void)osi_trace_console_trap_queue_put(node);
    }
    osi_mutex_Unlock(&osi_trace_console_trap_queue.lock);

    osi_mutex_Destroy(&osi_trace_console_trap_queue.lock);
    osi_condvar_Destroy(&osi_trace_console_trap_queue.cv);
    osi_mem_object_cache_destroy(osi_trace_console_trap_queue_cache);

    return res;
}
