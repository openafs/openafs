/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_condvar.h>
#include <trace/activation.h>
#include <trace/gen_rgy.h>
#include <trace/USERSPACE/gen_rgy.h>
#include <trace/consumer/activation.h>
#include <trace/consumer/cursor.h>
#include <trace/consumer/gen_rgy.h>
#include <trace/consumer/config.h>
#include <trace/cursor.h>
#include <trace/syscall.h>
#include <trace/agent/console.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <trace/rpc/agent_api.h>

/*
 * osi tracing framework
 * trace buffer read cursor
 */

afs_int32
SAgent_ProbeEnable(struct rx_call * tcall, 
		   osi_Trace_rpc_console_handle * console_handle,
		   osi_Trace_rpc_proc_handle proc_handle,
		   char * filter,
		   afs_uint32 * nhits)
{
    osi_result code;
    osi_trace_gen_id_t gen_id = (osi_trace_gen_id_t) proc_handle;

    code = osi_trace_console_rgy_probe_enable(console_handle,
					      gen_id,
					      filter,
					      nhits);

    return OSI_RESULT_RPC_CODE(code);
}

afs_int32
SAgent_ProbeDisable(struct rx_call * tcall, 
		    osi_Trace_rpc_console_handle * console_handle,
		    osi_Trace_rpc_proc_handle proc_handle,
		    char * filter,
		    afs_uint32 * nhits)
{
    osi_result code;
    osi_trace_gen_id_t gen_id = (osi_trace_gen_id_t) proc_handle;

    code = osi_trace_console_rgy_probe_disable(console_handle,
					       gen_id,
					       filter,
					       nhits);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }


 error:
    return OSI_RESULT_RPC_CODE(code);
}
