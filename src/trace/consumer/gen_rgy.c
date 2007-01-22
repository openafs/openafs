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
 * generator registry
 * userspace syscall interface
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <trace/directory.h>
#include <trace/syscall.h>
#include <trace/consumer/gen_rgy.h>

osi_result
osi_trace_gen_rgy_sys_lookup_I2A(osi_trace_gen_id_t gen_id,
				 osi_trace_generator_address_t * addr)
{
    int rv;
    osi_result res;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_I2A, 
			    gen_id, 
			    addr, 
			    0,
			    &rv);

    return res;
}

osi_result
osi_trace_gen_rgy_sys_lookup_A2I(osi_trace_generator_address_t * addr,
				 osi_trace_gen_id_t * gen_id)
{
    int rv;
    osi_result res;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_A2I, 
			    addr, 
			    gen_id, 
			    0,
			    &rv);

    return res;
}
