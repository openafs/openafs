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
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <trace/generator/cursor.h>
#include <trace/syscall.h>
#include <trace/generator/directory.h>
#include <osi/osi_kernel.h>
#include <osi/osi_syscall.h>
#include <osi/osi_proc.h>
#include <osi/osi_string.h>
#include <trace/KERNEL/config.h>
#include <trace/KERNEL/cursor.h>
#include <trace/KERNEL/gen_rgy.h>
#include <trace/KERNEL/postmaster.h>
#include <trace/generator/module.h>

/*
 * osi tracing framework
 * trace syscall
 */


#if (OSI_DATAMODEL_IS(OSI_ILP32_ENV))
#define osi_Trace_sys_datamodel_switch(sys,sys32,args)  (sys args)
#else /* !OSI_DATAMODEL_IS(OSI_ILP32_ENV) */
#define osi_Trace_sys_datamodel_switch(sys,sys32,args) \
    ((osi_syscall_do_32u_64k_trans()) ? \
     sys32 args : \
     sys args)
#endif /* !OSI_DATAMODEL_IS(OSI_ILP32_ENV) */

/*
 * main osi_Trace syscall handler
 */
int
osi_Trace_syscall_handler(long opcode, 
			  long p1, long p2, long p3, long p4, 
			  long * retVal)
{
    int code = 0;
    osi_result res;
    char probe_name[OSI_TRACE_MAX_PROBE_NAME_LEN];
    osi_trace_probe_id_t probe_id;
    size_t bufferlen;

    /*
     * for now we require superuser privs to
     * make any osi_trace syscall
     */
    osi_syscall_suser_check(&code);
    if (code) {
	return code;
    }

    switch (opcode) {
    case OSI_TRACE_SYSCALL_OP_INSERT:
	res = osi_TraceFunc_UserTraceTrap((osi_TracePoint_record *)p1);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = EAGAIN;
	}
	break;
    case OSI_TRACE_SYSCALL_OP_READ:
	code = osi_trace_cursor_sys_read(p1,p2,p3,retVal);
	break;
    case OSI_TRACE_SYSCALL_OP_READV:
	code = osi_Trace_sys_datamodel_switch(osi_trace_cursor_sys_readv,
					      osi_trace_cursor_sys32_readv,
					      (p1,p2,p3,retVal));
	break;
    case OSI_TRACE_SYSCALL_OP_MAIL_SEND:
	code = osi_Trace_sys_datamodel_switch(osi_trace_mail_sys_send,
					      osi_trace_mail_sys32_send,
					      ((void *)p1));
	break;
    case OSI_TRACE_SYSCALL_OP_MAIL_SENDV:
	code = osi_Trace_sys_datamodel_switch(osi_trace_mail_sys_sendv,
					      osi_trace_mail_sys32_sendv,
					      ((void *)p1));
	break;
    case OSI_TRACE_SYSCALL_OP_MAIL_CHECK:
	code = osi_Trace_sys_datamodel_switch(osi_trace_mail_sys_check,
					      osi_trace_mail_sys32_check,
					      ((osi_trace_gen_id_t)p1,
					       (void *)p2,
					       (osi_uint32)p3));
	break;
    case OSI_TRACE_SYSCALL_OP_MAIL_CHECKV:
	code = osi_Trace_sys_datamodel_switch(osi_trace_mail_sys_checkv,
					      osi_trace_mail_sys32_checkv,
					      ((osi_trace_gen_id_t)p1,
					       (void *)p2,
					       (osi_uint32)p3,
					       retVal));
	break;
    case OSI_TRACE_SYSCALL_OP_ENABLE:
	osi_kernel_copy_in_string(p1, probe_name, sizeof(probe_name), &bufferlen, &code);
	if (osi_compiler_expect_true(!code)) {
	    res = osi_trace_probe_enable_by_filter(probe_name);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		code = EINVAL;
	    }
	}
	break;
    case OSI_TRACE_SYSCALL_OP_DISABLE:
	osi_kernel_copy_in_string(p1, probe_name, sizeof(probe_name), &bufferlen, &code);
	if (osi_compiler_expect_true(!code)) {
	    res = osi_trace_probe_disable_by_filter(probe_name);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		code = EINVAL;
	    }
	}
	break;
    case OSI_TRACE_SYSCALL_OP_ENABLE_BY_ID:
	res = osi_trace_probe_enable_by_id((osi_trace_probe_id_t)p1);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = EAGAIN;
	}
	break;
    case OSI_TRACE_SYSCALL_OP_DISABLE_BY_ID:
	res = osi_trace_probe_disable_by_id((osi_trace_probe_id_t)p1);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = EAGAIN;
	}
	break;
    case OSI_TRACE_SYSCALL_OP_LOOKUP_N2I:
	osi_kernel_copy_in_string(p1, probe_name, sizeof(probe_name), &bufferlen, &code);
	if (osi_compiler_expect_true(!code)) {
	    osi_trace_probe_t probe;
	    res = osi_trace_directory_N2P(probe_name, &probe);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		code = EINVAL;
	    } else {
		res = osi_trace_probe_id(probe, &probe_id);
		if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		    code = EINVAL;
		} else {
		    osi_kernel_copy_out(&probe_id, p2, sizeof(probe_id), &code);
		}
	    }
	}
	break;
    case OSI_TRACE_SYSCALL_OP_LOOKUP_I2N:
	{
	    char * name_out;
	    int slen;
	    size_t out_len;
	    name_out = (char *) osi_mem_alloc(OSI_TRACE_MAX_PROBE_NAME_LEN);
	    res = osi_trace_directory_I2N((osi_trace_probe_id_t) p1, name_out, OSI_TRACE_MAX_PROBE_NAME_LEN);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		code = EINVAL;
	    } else {
		slen = osi_string_len(name_out) + 1;
		if (osi_compiler_expect_false(slen > out_len)) {
		    code = EINVAL;
		} else {
		    osi_kernel_copy_out(name_out, p2, slen, &code);
		}
	    }
	    osi_mem_free(name_out, OSI_TRACE_MAX_PROBE_NAME_LEN);
	}
	break;
    case OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_I2A:
	code = osi_trace_gen_rgy_sys_lookup_I2A(p1, (void *)p2);
	break;
    case OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_A2I:
	code = osi_trace_gen_rgy_sys_lookup_A2I((void *)p1, (void *)p2);
	break;
    case OSI_TRACE_SYSCALL_OP_GEN_GET:
	code = osi_trace_gen_rgy_sys_get(p1);
	break;
    case OSI_TRACE_SYSCALL_OP_GEN_GET_BY_ADDR:
	code = osi_trace_gen_rgy_sys_get_by_addr((void *)p1, (void *)p2);
	break;
    case OSI_TRACE_SYSCALL_OP_GEN_PUT:
	code = osi_trace_gen_rgy_sys_put(p1);
	break;
    case OSI_TRACE_SYSCALL_OP_GEN_REGISTER:
	code = osi_trace_gen_rgy_sys_register((void *)p1, (void *)p2);
	break;
    case OSI_TRACE_SYSCALL_OP_GEN_UNREGISTER:
	code = osi_trace_gen_rgy_sys_unregister(p1);
	break;
    case OSI_TRACE_SYSCALL_OP_CONFIG_GET:
	code = osi_Trace_sys_datamodel_switch(osi_trace_config_sys_get,
					      osi_trace_config_sys32_get,
					      (p1));
	break;
    case OSI_TRACE_SYSCALL_OP_MODULE_INFO:
	code = osi_trace_module_sys_info((void *)p1);
	break;
    case OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP:
	code = osi_trace_module_sys_lookup(p1, (void *)p2);
	break;
    case OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP_BY_NAME:
	code = osi_trace_module_sys_lookup_by_name((const char *)p1, (void *)p2);
	break;
    case OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP_BY_PREFIX:
	code = osi_trace_module_sys_lookup_by_prefix((const char *)p1, (void *)p2);
	break;
    case OSI_TRACE_SYSCALL_OP_MAIL_TAP:
	code = osi_trace_mail_sys_tap(p1, p2);
	break;
    case OSI_TRACE_SYSCALL_OP_MAIL_UNTAP:
	code = osi_trace_mail_sys_untap(p1, p2);
	break;
    case OSI_TRACE_SYSCALL_OP_NULL:
	break;
    default:
	code = ENOSYS;
    }

    return code;
}


osi_result
osi_trace_syscall_PkgInit(void)
{
    return OSI_OK;
}

osi_result
osi_trace_syscall_PkgShutdown(void)
{
    return OSI_OK;
}
