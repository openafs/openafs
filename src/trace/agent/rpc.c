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
#include <trace/gen_rgy.h>
#include <trace/USERSPACE/gen_rgy.h>
#include <trace/consumer/gen_rgy.h>
#include <trace/consumer/config.h>
#include <trace/cursor.h>
#include <trace/syscall.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <trace/rpc/agent_api.h>

/*
 * osi tracing framework
 * trace agent library
 * general rpc stubs
 */

osi_static void fill_proc_info(osi_Trace_rpc_proc_info *,
			       osi_trace_gen_id_t,
			       osi_trace_generator_address_t *);


afs_int32
SAgent_GetVersion(struct rx_call * tcall, 
		  afs_uint32 * ver, 
		  afs_uint32 * build_date)
{
    *ver = 1;
    *build_date = 0;
    return OSI_OK;
}

afs_int32
SAgent_AgentInfo4(struct rx_call * tcall,
		  osi_Trace_rpc_agent_handle * self,
		  osi_Trace_rpc_bulkaddrs4 * addrs)
{
    return RXGEN_OPCODE;
}

afs_int32
SAgent_ProcSelf(struct rx_call * tcall,
		osi_Trace_rpc_proc_handle * self)
{
    osi_result code;
    osi_trace_gen_id_t gen;

    code = osi_trace_gen_id(&gen);
    *self = (osi_Trace_rpc_proc_handle) gen;

    return OSI_RESULT_RPC_CODE(code);
}

afs_int32
SAgent_ProcList(struct rx_call * tcall, 
		osi_Trace_rpc_proc_handle start_index,
		afs_uint32 * nentries, 
		osi_Trace_rpc_proc_info_vec * entries)
{
    osi_result code;
    osi_trace_gen_id_t gen;
    osi_uint32 filled, vec_len, max_id;
    osi_trace_generator_address_t addr;

    vec_len = *nentries;
    if (vec_len > OSI_TRACE_RPC_PROC_VEC_MAX) {
	vec_len = OSI_TRACE_RPC_PROC_VEC_MAX;
    }

    entries->osi_Trace_rpc_proc_info_vec_val = osi_NULL;

    /* get the max gen id supported by the kernel gen rgy data structures */
    code = osi_trace_consumer_kernel_config_lookup(OSI_TRACE_KERNEL_CONFIG_GEN_RGY_MAX_ID,
						   &max_id);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    /* allocate a response vector */
    entries->osi_Trace_rpc_proc_info_vec_val = (osi_Trace_rpc_proc_info *)
	osi_mem_alloc(vec_len * sizeof(osi_Trace_rpc_proc_info));
    if (osi_compiler_expect_false(entries->osi_Trace_rpc_proc_info_vec_val == osi_NULL)) {
	code = OSI_ERROR_NOMEM;
	goto error;
    }
    entries->osi_Trace_rpc_proc_info_vec_len = vec_len;

    filled = 0;
    gen = (osi_trace_gen_id_t) start_index;
    for (; (gen < max_id) && (filled < vec_len); gen++) {
	code = osi_trace_gen_rgy_sys_lookup_I2A(gen, &addr);
	if (OSI_RESULT_OK(code)) {
	    fill_proc_info(&entries->osi_Trace_rpc_proc_info_vec_val[filled],
			   gen, &addr);
	    filled++;
	}
    }

    if (filled < vec_len) {
	entries->osi_Trace_rpc_proc_info_vec_val = (osi_Trace_rpc_proc_info *)
	    osi_mem_realloc(entries->osi_Trace_rpc_proc_info_vec_val,
			    filled * sizeof(osi_Trace_rpc_proc_info));
	entries->osi_Trace_rpc_proc_info_vec_len = filled;
    }
    *nentries = filled;

 done:
    return OSI_RESULT_RPC_CODE(code);

 error:
    if (entries->osi_Trace_rpc_proc_info_vec_val) {
	osi_mem_free(entries->osi_Trace_rpc_proc_info_vec_val,
		     entries->osi_Trace_rpc_proc_info_vec_len);
	entries->osi_Trace_rpc_proc_info_vec_val = osi_NULL;
    }
    entries->osi_Trace_rpc_proc_info_vec_len = *nentries = 0;
    goto done;
}

afs_int32
SAgent_ProcInfo(struct rx_call * tcall, 
		osi_Trace_rpc_proc_handle index,
		osi_Trace_rpc_proc_info * info)
{
    osi_result code;
    osi_trace_gen_id_t gen;
    osi_trace_generator_address_t addr;

    gen = (osi_trace_gen_id_t) index;
    code = osi_trace_gen_rgy_sys_lookup_I2A(gen, &addr);
    if (OSI_RESULT_OK(code)) {
	fill_proc_info(info, gen, &addr);
    }

 done:
    return OSI_RESULT_RPC_CODE(code);
}

osi_static void 
fill_proc_info(osi_Trace_rpc_proc_info * info,
	       osi_trace_gen_id_t gen,
	       osi_trace_generator_address_t * addr)
{
    info->handle = gen;
    info->pid = addr->pid;
    info->programType = addr->programType;

    /* give a meaningless value until we start querying this properly */
    info->trace_interface_version = 0;

    osi_mem_zero(info->capabilities, sizeof(info->capabilities));
}

