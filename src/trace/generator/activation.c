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
#include <trace/activation.h>
#include <trace/generator/activation.h>
#include <trace/generator/activation_impl.h>
#include <trace/common/options.h>

/*
 * osi tracing framework
 * tracepoint activation control
 */

osi_result
osi_TracePoint_Activate(osi_trace_probe_id_t id)
{
    osi_fast_uint offset, bitnum, mask;
    osi_result res = OSI_OK;

    osi_Trace_Probe2Index(id, offset, bitnum);
    mask = 1 << bitnum;

#if defined(OSI_TRACE_ACTIVATION_ATOMIC_OPS)
    osi_atomic_or_fast((volatile osi_fast_uint *)(&_osi_tracepoint_config._tp_activation_vec[offset]), 
		       mask);
#else /* !OSI_TRACE_ACTIVATION_ATOMIC_OPS */
    osi_mutex_Lock(&_osi_tracepoint_config.lock);
    _osi_tracepoint_config._tp_activation_vec[offset] |= mask;
    osi_mutex_Unlock(&_osi_tracepoint_config.lock);
#endif /* !OSI_TRACE_ACTIVATION_ATOMIC_OPS */

    return OSI_OK;
}

osi_result
osi_TracePoint_Deactivate(osi_trace_probe_id_t id)
{
    osi_fast_uint offset, bitnum, mask;
    osi_result res = OSI_OK;

    osi_Trace_Probe2Index(id, offset, bitnum);
    mask = ~(1 << bitnum);

#if defined(OSI_TRACE_ACTIVATION_ATOMIC_OPS)
    osi_atomic_and_fast((volatile osi_fast_uint *)(&_osi_tracepoint_config._tp_activation_vec[offset]),
			mask);
#else /* !OSI_TRACE_ACTIVATION_ATOMIC_OPS */
    osi_mutex_Lock(&_osi_tracepoint_config.lock);
    _osi_tracepoint_config._tp_activation_vec[offset] &= mask;
    osi_mutex_Unlock(&_osi_tracepoint_config.lock);
#endif /* !OSI_TRACE_ACTIVATION_ATOMIC_OPS */

    return OSI_OK;
}


osi_result
osi_trace_activation_PkgInit(void)
{
    osi_result res = OSI_OK;
    void * buf;
    size_t align, len;

    osi_mutex_Init(&_osi_tracepoint_config.lock,
		   &osi_trace_common_options.mutex_opts);

    if (OSI_RESULT_FAIL(osi_cache_max_alignment(&align))) {
	align = 64;
    }

    len = OSI_TRACE_ACTIVATION_VEC_LEN;
    if (len & (align-1)) {
	len &= ~(align-1);
	len += align;
    }

    buf = osi_mem_aligned_alloc_nosleep(len, align);

    if (osi_compiler_expect_false(buf == osi_NULL)) {
	_osi_tracepoint_config._tp_activation_vec = osi_NULL;
	_osi_tracepoint_config._tp_activation_vec_len = 0;
	res = OSI_FAIL;
	goto error;
    } else {
	osi_mem_zero(buf, len);
	_osi_tracepoint_config._tp_activation_vec = buf;
	_osi_tracepoint_config._tp_activation_vec_len = len;
    }

 error:
    return res;
}

osi_result
osi_trace_activation_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    /* deactivate all tracepoints */
    osi_mutex_Lock(&_osi_tracepoint_config.lock);
    osi_mem_zero((void *)_osi_tracepoint_config._tp_activation_vec,
		 _osi_tracepoint_config._tp_activation_vec_len);

    osi_mem_aligned_free((void *)_osi_tracepoint_config._tp_activation_vec,
			 _osi_tracepoint_config._tp_activation_vec_len);
    osi_mutex_Unlock(&_osi_tracepoint_config.lock);
    osi_mutex_Destroy(&_osi_tracepoint_config.lock);

    return res;
}
