/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


/*
 * osi tracing framework
 * generator registry
 * kernel support
 */

#include <trace/common/trace_impl.h>
#include <trace/directory.h>
#include <osi/osi_proc.h>
#include <osi/osi_rwlock.h>
#include <trace/syscall.h>
#include <trace/USERSPACE/gen_rgy.h>

osi_static osi_trace_gen_id_t _osi_trace_gen_id = 0;

/*
 * register us as a trace endpoint with the kernel module
 *
 * [IN] gen_addr  -- generator address information
 * [OUT] gen_id   -- gen_id assigned by the kernel
 *
 * returns:
 *   see osi_Trace_syscall()
 */
osi_result
osi_trace_gen_rgy_register(osi_trace_generator_address_t * gen_addr,
			   osi_trace_gen_id_t * gen_id)
{
    int rv;
    osi_result res;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_REGISTER, 
			    (long) gen_addr, 
			    (long) gen_id, 
			    0,
			    &rv);

    return res;
}

/*
 * unregister an assigned gen id
 *
 * [IN] gen_id  -- gen_id previously assigned by kernel module
 *
 * returns:
 *   see osi_Trace_syscall()
 */
osi_result
osi_trace_gen_rgy_unregister(osi_trace_gen_id_t gen_id)
{
    int rv;
    osi_result res;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_UNREGISTER, 
			    (long) gen_id, 
			    0, 
			    0,
			    &rv);

    return res;
}

/*
 * hold a reference on a peer's gen_id
 *
 * [IN] gen_id  -- gen_id of process on which to grab a ref
 *
 * returns:
 *   see osi_Trace_syscall()
 */
osi_result
osi_trace_gen_rgy_get(osi_trace_gen_id_t gen_id)
{
    int rv;
    osi_result res;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_GET, 
			    (long) gen_id, 
			    0, 
			    0,
			    &rv);

    return res;
}

/*
 * release a reference on a peer's gen_id
 *
 * [IN] gen_id  -- gen_id of process on which to release ref
 *
 * returns:
 *   see osi_Trace_syscall()
 */
osi_result
osi_trace_gen_rgy_put(osi_trace_gen_id_t gen_id)
{
    int rv;
    osi_result res;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_PUT, 
			    (long) gen_id, 
			    0, 
			    0,
			    &rv);

    return res;
}

/*
 * get a reference to a gen_id based upon an address structure
 *
 * [IN] addr     -- generator address to look up
 * [OUT] gen_id  -- address in which to store generator address
 *
 * returns:
 *   see osi_Trace_syscall()
 */
osi_result
osi_trace_gen_rgy_get_by_addr(osi_trace_generator_address_t * addr,
			      osi_trace_gen_id_t * gen_id)
{
    int rv;
    osi_result res;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_GET_BY_ADDR,
			    (long) addr,
			    (long) gen_id,
			    0,
			    &rv);

    return res;
}

osi_result
osi_trace_gen_rgy_PkgInit(void)
{
    osi_result res = OSI_OK;
    osi_trace_generator_address_t addr;
    osi_options_val_t opt;

    res = osi_config_options_Get(OSI_OPTION_TRACE_GEN_RGY_REGISTRATION,
				 &opt);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	(osi_Msg "%s: failed to get value for option TRACE_GEN_RGY_REGISTRATION\n",
	 __osi_func__);
	res = OSI_FAIL;
	goto error;
    }

    if (opt.val.v_bool == OSI_TRUE) {
	addr.programType = osi_config_programType();
	addr.pid = osi_proc_current_id();
	res = osi_trace_gen_rgy_register(&addr, &_osi_trace_gen_id);
    }

 error:
    return res;
}

osi_result
osi_trace_gen_rgy_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    if (_osi_trace_gen_id) {
	res = osi_trace_gen_rgy_unregister(_osi_trace_gen_id);
    }

    return res;
}

/*
 * get our current gen_id
 *
 * [OUT] id  -- address in which to store our id
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_gen_id(osi_trace_gen_id_t * id)
{
    *id = _osi_trace_gen_id;
    return OSI_OK;
}

/*
 * set our current gen_id
 *
 * [IN] id  -- new gen_id
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_gen_id_set(osi_trace_gen_id_t id)
{
    _osi_trace_gen_id = id;
    return OSI_OK;
}
