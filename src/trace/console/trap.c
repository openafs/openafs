/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_list.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_mutex.h>
#include <trace/console/trap.h>
#include <trace/console/trap_impl.h>
#include <trace/common/options.h>

/*
 * osi tracing framework
 * trace console
 * types for trap support
 */

typedef struct {
    osi_list_element_volatile handler_list;
    osi_trace_console_trap_handler_t * handler;
    void * sdata;
} osi_trace_console_trap_handler_node_t;


struct osi_trace_console_trap_handler_rgy {
    osi_list_head_volatile handlers;
    osi_mutex_t lock;
};

struct osi_trace_console_trap_handler_rgy osi_trace_console_trap_handler_rgy;

osi_mem_object_cache_t * osi_trace_console_trap_handler_cache = osi_NULL;

osi_result
osi_trace_console_trap_handler_register(osi_trace_console_trap_handler_t * handler,
					void * sdata)
{
    osi_result res = OSI_OK;
    osi_trace_console_trap_handler_node_t * node;

    node = (osi_trace_console_trap_handler_node_t *)
	osi_mem_object_cache_alloc(osi_trace_console_trap_handler_cache);
    if (osi_compiler_expect_false(node == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    node->handler = handler;
    node->sdata = sdata;

    osi_mutex_Lock(&osi_trace_console_trap_handler_rgy.lock);
    osi_list_Append(&osi_trace_console_trap_handler_rgy.handlers,
		    node,
		    osi_trace_console_trap_handler_node_t,
		    handler_list);
    osi_mutex_Unlock(&osi_trace_console_trap_handler_rgy.lock);

 error:
    return res;
}

osi_result
osi_trace_console_trap_handler_unregister(osi_trace_console_trap_handler_t * handler)
{
    osi_result res = OSI_FAIL;
    osi_trace_console_trap_handler_node_t * node, * nn;

    osi_mutex_Lock(&osi_trace_console_trap_handler_rgy.lock);
    for (osi_list_Scan(&osi_trace_console_trap_handler_rgy.handlers,
		       node, nn,
		       osi_trace_console_trap_handler_node_t,
		       handler_list)) {
	if (node->handler == handler) {
	    osi_list_Remove(node,
			    osi_trace_console_trap_handler_node_t,
			    handler_list);
	    res = OSI_OK;
	    node->handler = osi_NULL;
	    node->sdata = osi_NULL;
	    osi_mem_object_cache_free(osi_trace_console_trap_handler_cache, node);
	    break;
	}
    }
    osi_mutex_Unlock(&osi_trace_console_trap_handler_rgy.lock);

    return res;
}

osi_result
osi_trace_console_trap_handle(osi_trace_console_trap_t * trap)
{
    osi_result res, code = OSI_OK;
    osi_trace_console_trap_handler_node_t * node;

    osi_mutex_Lock(&osi_trace_console_trap_handler_rgy.lock);
    for (osi_list_Scan_Immutable(&osi_trace_console_trap_handler_rgy.handlers,
		       node,
		       osi_trace_console_trap_handler_node_t,
		       handler_list)) {
	res = (*node->handler)(trap, node->sdata);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }
    osi_mutex_Unlock(&osi_trace_console_trap_handler_rgy.lock);

    return code;
}

osi_result
osi_trace_console_trap_handler_PkgInit(void)
{
    osi_result res = OSI_OK;

    osi_mutex_Init(&osi_trace_console_trap_handler_rgy.lock,
		   &osi_trace_common_options.mutex_opts);
    osi_list_Init(&osi_trace_console_trap_handler_rgy.handlers);

    osi_trace_console_trap_handler_cache =
	osi_mem_object_cache_create("osi_trace_console_trap_handler",
				    sizeof(osi_trace_console_trap_handler_node_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_compiler_expect_false(osi_trace_console_trap_handler_cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

osi_result
osi_trace_console_trap_handler_PkgShutdown(void)
{
    osi_result res = OSI_OK;
    osi_trace_console_trap_handler_node_t * node, * nn;

    osi_mutex_Lock(&osi_trace_console_trap_handler_rgy.lock);
    for (osi_list_Scan(&osi_trace_console_trap_handler_rgy.handlers,
		       node, nn,
		       osi_trace_console_trap_handler_node_t,
		       handler_list)) {
	osi_list_Remove(node,
			osi_trace_console_trap_handler_node_t,
			handler_list);
	node->handler = osi_NULL;
	node->sdata = osi_NULL;
	osi_mem_object_cache_free(osi_trace_console_trap_handler_cache, node);
    }
    osi_mutex_Unlock(&osi_trace_console_trap_handler_rgy.lock);

    osi_mutex_Destroy(&osi_trace_console_trap_handler_rgy.lock);
    osi_mem_object_cache_destroy(osi_trace_console_trap_handler_cache);

    return res;
}
