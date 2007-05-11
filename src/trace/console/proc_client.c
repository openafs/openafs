/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_object_cache.h>
#include <trace/gen_rgy.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <trace/rpc/agent_api.h>
#include <trace/console/proc_client.h>
#include <trace/console/proc_client_impl.h>

/*
 * osi tracing framework
 * trace console library
 * agent instrumented process query rpc interface 
 * low-level client routines
 */

OSI_TRACE_CONSOLE_PROC_ITERATOR_STATIC_PROTOTYPE(osi__trace__console__proc___iterator);

osi_extern struct rx_securityClass * osi_trace_console_rpc_C_sc_vec[1];
osi_extern struct rx_securityClass * osi_trace_console_rpc_S_sc_vec[1];
osi_extern struct rx_service * osi_trace_console_rpc_service;

osi_extern osi_Trace_rpc_console_handle osi_trace_console_uuid;

osi_mem_object_cache_t * osi_trace_console_proc_list_node_cache = osi_NULL;

/*
 * get the process handle id for the agent process on the remote agent node
 *
 * [IN] addr      -- address of agent to contact
 * [OUT] proc_id  -- address in which to store agent process handle
 *
 * returns:
 *   OSI_FAIL on rx_GetCachedConnection failure
 *   see Agent_ProcSelf()
 */
osi_result
osi_trace_console_proc_agent(osi_Trace_rpc_addr4 * addr,
			     osi_Trace_rpc_proc_handle * proc_id)
{
    osi_result res;
    afs_int32 code;
    struct rx_connection * conn;

    conn = rx_GetCachedConnection(addr->addr,
				  addr->port,
				  OSI_TRACE_RPC_SERVICE_AGENT,
				  osi_trace_console_rpc_C_sc_vec,
				  0);
    if (osi_compiler_expect_false(conn == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    code = Agent_ProcSelf(conn,
			  proc_id);
    res = (osi_result) code;

    rx_ReleaseCachedConnection(conn);

 error:
    return res;
}

/*
 * get information on a remote instrumented process
 *
 * [IN] addr     -- address of agent to contact
 * [IN] proc_id  -- remote process handle
 * [OUT] info    -- address in which to store process info
 *
 * returns:
 *   OSI_FAIL on rx_GetCachedConnection failure
 *   see Agent_ProcInfo()
 */
osi_result
osi_trace_console_proc_info(osi_Trace_rpc_addr4 * addr,
			    osi_Trace_rpc_proc_handle proc_id,
			    osi_Trace_rpc_proc_info * info)
{
    osi_result res;
    afs_int32 code;
    struct rx_connection * conn;

    conn = rx_GetCachedConnection(addr->addr,
				  addr->port,
				  OSI_TRACE_RPC_SERVICE_AGENT,
				  osi_trace_console_rpc_C_sc_vec,
				  0);
    if (osi_compiler_expect_false(conn == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    code = Agent_ProcInfo(conn,
			  proc_id,
			  info);
    res = (osi_result) code;

    rx_ReleaseCachedConnection(conn);

 error:
    return res;
}

/*
 * iterate over all processes on a remote agent
 *
 * [IN] addr  -- address of agent to contact
 * [IN] fp    -- iterator function pointer
 * [IN] rock  -- rock to pass into iterator
 *
 * returns:
 *   OSI_FAIL on rx_GetCachedConnection failure
 *   see Agent_ProcList()
 */
osi_result
osi_trace_console_proc_foreach(osi_Trace_rpc_addr4 * addr,
			       osi_trace_console_proc_iterator_t * fp,
			       void * rock)
{
    osi_result res = OSI_OK;
    afs_int32 code;
    afs_uint32 nentries;
    osi_Trace_rpc_proc_handle idx;
    osi_Trace_rpc_proc_info_vec info_vec;
    osi_Trace_rpc_proc_info * info;
    struct rx_connection * conn;
    osi_uint32 i;

    conn = rx_GetCachedConnection(addr->addr,
				  addr->port,
				  OSI_TRACE_RPC_SERVICE_AGENT,
				  osi_trace_console_rpc_C_sc_vec,
				  0);
    if (osi_compiler_expect_false(conn == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    for (idx = 0; OSI_RESULT_OK(res); ) {
	nentries = OSI_TRACE_RPC_PROC_VEC_MAX;
	code = Agent_ProcList(conn,
			      idx,
			      &nentries,
			      &info_vec);
	res = (osi_result) code;
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    break;
	}

	if (!nentries) {
	    break;
	}

	info = (osi_Trace_rpc_proc_info *) 
	    info_vec.osi_Trace_rpc_proc_info_vec_val;
	for (i = 0; i < info_vec.osi_Trace_rpc_proc_info_vec_len; i++) {
	    res = (*fp)(addr, &info[i], rock);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		break;
	    }
	}	    
    }

    rx_ReleaseCachedConnection(conn);

 error:
    return res;
}

/*
 * remote process iteration handler
 */
OSI_TRACE_CONSOLE_PROC_ITERATOR_STATIC_DECL(osi_trace_console_proc_iterator)
{
    osi_result res = OSI_OK;
    osi_list_head * procs = OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_ROCK;
    osi_trace_console_proc_list_node_t * node;
    
    node = osi_mem_object_cache_alloc(osi_trace_console_proc_list_node_cache);
    if (osi_compiler_expect_false(node == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_mem_copy(&node->agent,
		 OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_AGENT,
		 sizeof(osi_Trace_rpc_addr4));
    osi_mem_copy(&node->proc_info,
		 OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_INFO,
		 sizeof(osi_Trace_rpc_proc_info));

    osi_list_Append(procs,
		    node,
		    osi_trace_console_proc_list_node_t,
		    proc_list);

 error:
    return res;
}

/*
 * iterate over all processes on a remote agent
 *
 * [IN] addr             -- address of agent to contact
 * [IN] proc_vec         -- remote process info vector
 * [INOUT] proc_vec_len  -- pass in info vector length, pass out number of entries stored
 *
 * returns:
 *   see osi_trace_console_proc_foreach()
 */
osi_result
osi_trace_console_proc_list(osi_Trace_rpc_addr4 * agent,
			    osi_Trace_rpc_proc_info * proc_vec,
			    osi_size_t proc_vec_len)
{
    osi_result res;
    osi_list_head proc_list;
    osi_trace_console_proc_list_node_t * node, * nnode;
    osi_uint32 i;

    osi_list_Init_Head(&proc_list);

    res = osi_trace_console_proc_foreach(agent,
					 &osi_trace_console_proc_iterator,
					 &proc_list);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    i = 0;
    for (osi_list_Scan(&proc_list,
		       node, nnode,
		       osi_trace_console_proc_list_node_t,
		       proc_list)) {
	if (i < proc_vec_len) {
	    osi_mem_copy(&proc_vec[i],
			 &node->proc_info,
			 sizeof(osi_Trace_rpc_proc_info));
	}
	i++;
	osi_list_Remove(node,
			osi_trace_console_proc_list_node_t,
			proc_list);
	osi_mem_object_cache_free(osi_trace_console_proc_list_node_cache,
				  node);
    }

 error:
    return res;
}


OSI_INIT_FUNC_DECL(osi_trace_console_proc_client_PkgInit)
{
    osi_result res = OSI_OK;

    osi_trace_console_proc_list_node_cache =
	osi_mem_object_cache_create("osi_trace_console_proc_list_node",
				    sizeof(osi_trace_console_proc_list_node_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_trace_console_proc_list_node_cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

OSI_FINI_FUNC_DECL(osi_trace_console_proc_client_PkgShutdown)
{
    osi_result res = OSI_OK;

    osi_mem_object_cache_destroy(osi_trace_console_proc_list_node_cache);

    return res;
}

