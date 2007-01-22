/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi.h>
#include <osi/osi_trace.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_condvar.h>
#include <trace/activation.h>
#include <trace/gen_rgy.h>
#include <trace/agent/console.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <trace/rpc/agent_api.h>

/*
 * osi tracing framework
 * trace agent library
 * predicate rpc server endpoints
 */

/*
 * the predicate mechanism is a future design goal.
 * for now it remains unimplemented.
 */

afs_int32
SAgent_PredicateRegister(struct rx_call * tcall,
			 osi_Trace_rpc_console_handle * console_handle,
			 osi_Trace_rpc_predicate_handle * predicate_handle,
			 char * predicate)
{
    return RXGEN_OPCODE;
}

afs_int32
SAgent_PredicateUnregister(struct rx_call * tcall,
			   osi_Trace_rpc_console_handle * console_handle,
			   osi_Trace_rpc_predicate_handle * predicate_handle)
{
    return RXGEN_OPCODE;
}

afs_int32
SAgent_PredicateAttach(struct rx_call * tcall,
		       osi_Trace_rpc_console_handle * console_handle,
		       osi_Trace_rpc_predicate_handle * predicate_handle,
		       osi_Trace_rpc_proc_handle proc_handle,
		       char * probe_filter,
		       afs_uint32 * nhits)
{
    return RXGEN_OPCODE;
}

afs_int32
SAgent_PredicateDetach(struct rx_call * tcall,
		       osi_Trace_rpc_console_handle * console_handle,
		       osi_Trace_rpc_predicate_handle * predicate_handle,
		       osi_Trace_rpc_proc_handle proc_handle,
		       char * probe_filter,
		       afs_uint32 * nhits)
{
    return RXGEN_OPCODE;
}
