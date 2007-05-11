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
#include <trace/generator/cursor.h>
#include <trace/generator/buffer.h>
#include <trace/syscall.h>
#include <trace/generator/directory.h>
#include <osi/osi_kernel.h>
#include <osi/osi_syscall.h>
#include <osi/osi_proc.h>
#include <trace/KERNEL/gen_rgy.h>
#include <trace/KERNEL/gen_rgy_impl.h>
#include <trace/KERNEL/postmaster.h>

/*
 * osi tracing framework
 * trace configuration get
 */

struct osi_trace_config_fill_t {
    osi_trace_syscall_config_t * cfg;
    osi_uint32 iter;
};

osi_static osi_result
osi_trace_config_add_el(struct osi_trace_config_fill_t * iter,
			osi_uint32 key,
			osi_uint32 value)
{
    osi_result res = OSI_OK;

    if (osi_compiler_expect_false(iter->iter >= iter->cfg->nelements)) {
	res = OSI_FAIL;
	goto error;
    }

    iter->cfg->buf[iter->iter].key = key;
    iter->cfg->buf[iter->iter].value = value;

    iter->iter++;

 error:
    return res;
}

osi_static osi_result
osi_trace_config_term(struct osi_trace_config_fill_t * iter)
{
    osi_result res = OSI_OK;

    if (osi_compiler_expect_false(!iter->cfg->nelements)) {
	res = OSI_FAIL;
	goto error;
    }

    if (iter->iter == iter->cfg->nelements) {
	iter->iter--;
    }

    iter->cfg->buf[iter->iter].key = OSI_TRACE_KERNEL_CONFIG_TERMINAL;
    iter->cfg->buf[iter->iter].value = 0;

 error:
    return res;
}

osi_result 
osi_trace_config_get(osi_trace_syscall_config_t * cfg)
{
    osi_result res;
    struct osi_trace_config_fill_t iter;
    size_t s;

    iter.cfg = cfg;
    iter.iter = 0;

    osi_mem_zero(cfg->buf, cfg->nelements * sizeof(osi_trace_syscall_config_element_t));

    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_SIZEOF_REGISTER, 
				  sizeof(osi_register_int));
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_SIZEOF_FAST, 
				  sizeof(osi_fast_int));
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_SIZEOF_LARGE, 
				  sizeof(osi_large_int));
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_SIZEOF_RECORD, 
				  sizeof(osi_TracePoint_record));
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_DATAMODEL, 
				  (osi_uint32) osi_datamodel());
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_TRACE_VERSION, 
				  OSI_TRACEPOINT_RECORD_VERSION);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_GEN_RGY_MAX_ID, 
				  OSI_TRACE_GEN_RGY_MAX_ID_KERNEL);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

#if defined(OSI_TRACE_BUFFER_CTX_LOCAL)
    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_CTX_LOCAL_TRACE, 1);
#else
    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_CTX_LOCAL_TRACE, 0);
#endif
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    if (OSI_RESULT_FAIL_UNLIKELY(osi_TraceBuffer_size_get(&s))) {
	goto error;
    }
    res = osi_trace_config_add_el(&iter, 
				  OSI_TRACE_KERNEL_CONFIG_BUFFER_LEN, 
				  (osi_uint32) s);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_config_term(&iter);

 error:
    return res;
}


/* syscall interface */
int
osi_trace_config_sys_get(long p1)
{
    osi_trace_syscall_config_t cfg;
    osi_trace_syscall_config_element_t * el;
    int code;

    osi_kernel_copy_in(p1, &cfg, sizeof(cfg), &code);
    if (osi_compiler_expect_false(code)) {
	goto error;
    }

    el = cfg.buf;
    cfg.buf = (osi_trace_syscall_config_element_t *)
	osi_mem_alloc(cfg.nelements * sizeof(osi_trace_syscall_config_element_t));
    if (osi_compiler_expect_false(cfg.buf == osi_NULL)) {
	code = ENOMEM;
	goto error;
    }

    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_config_get(&cfg))) {
	code = EINVAL;
	goto error;
    }

    osi_kernel_copy_out(cfg.buf, el, 
			cfg.nelements * sizeof(osi_trace_syscall_config_element_t), 
			&code);

 error:
    return code;
}

#if (!OSI_DATAMODEL_IS(OSI_ILP32_ENV))
int
osi_trace_config_sys32_get(long p1)
{
    osi_trace_syscall32_config_t cfg32;
    osi_trace_syscall_config_t cfg;
    int code;

    osi_kernel_copy_in(p1, &cfg32, sizeof(cfg32), &code);
    if (osi_compiler_expect_false(code)) {
	goto error;
    }

    cfg.buf = (osi_trace_syscall_config_element_t *)
	osi_mem_alloc(cfg.nelements * sizeof(osi_trace_syscall_config_element_t));
    if (osi_compiler_expect_false(cfg.buf == osi_NULL)) {
	code = ENOMEM;
	goto error;
    }

    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_config_get(&cfg))) {
	code = EINVAL;
	goto error;
    }

    osi_kernel_copy_out(cfg.buf, cfg32.buf, 
			cfg.nelements * sizeof(osi_trace_syscall_config_element_t), 
			&code);

 error:
    return code;
}
#endif /* !OSI_DATAMODEL_IS(OSI_ILP32_ENV) */
