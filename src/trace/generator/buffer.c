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
#include <osi/osi_mem_local.h>
#include <osi/osi_condvar.h>
#include <trace/directory.h>
#include <trace/generator/generator.h>
#include <trace/generator/buffer.h>
#include <trace/activation.h>


struct osi_TraceBuffer_config osi_TraceBuffer_config;

OSI_INIT_FUNC_STATIC_PROTOTYPE(osi_TraceBuffer_config_PkgInit);
OSI_FINI_FUNC_STATIC_PROTOTYPE(osi_TraceBuffer_config_PkgShutdown);

/*
 * osi tracing framework
 * circular trace buffer
 */


#ifdef OSI_TRACE_BUFFER_CTX_LOCAL
osi_static void
osi_TraceBuffer_dtor(void * addr)
{
    osi_TraceBuffer * buf = (osi_TraceBuffer *) addr;
    (void)osi_TraceBuffer_Destroy(buf);
}

OSI_INIT_FUNC_DECL(osi_trace_buffer_PkgInit)
{
    osi_result res;

    res = osi_TraceBuffer_config_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_mem_local_key_create(&_osi_tracepoint_config._tpbuf_key,
				   &osi_TraceBuffer_dtor);

 error:
    return res;
}

OSI_FINI_FUNC_DECL(osi_trace_buffer_PkgShutdown)
{
    osi_result res;

    res = osi_TraceBuffer_config_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_mem_local_key_destroy(_osi_tracepoint_config._tpbuf_key);

 error:
    return res;
}

#else /* !OSI_TRACE_BUFFER_CTX_LOCAL */

osi_TraceBuffer * _osi_tracebuf;

OSI_INIT_FUNC_DECL(osi_trace_buffer_PkgInit)
{
    osi_result res;

    res = osi_TraceBuffer_config_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_TraceBuffer_Init(osi_NULL);

 error:
    return res;
}

OSI_FINI_FUNC_DECL(osi_trace_buffer_PkgShutdown)
{
    osi_result res;

    res = osi_TraceBuffer_config_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    if (_osi_tracebuf) {
	res = osi_TraceBuffer_Destroy(_osi_tracebuf);
    }

 error:
    return res;
}

#endif /* !OSI_TRACE_BUFFER_CTX_LOCAL */

osi_result
osi_TraceBuffer_Init(osi_Trace_EventHandle * handle)
{
    osi_TraceBuffer * buf;
    osi_size_t align, len;

    /*
     * allocate a cache aligned trace buffer control structure
     */
    if (OSI_RESULT_FAIL(osi_cache_max_alignment(&align))) {
	align = 64;
    }

    buf = (osi_TraceBuffer *) osi_mem_aligned_alloc_nosleep(sizeof(osi_TraceBuffer), align);
    if (osi_compiler_expect_false(buf == osi_NULL)) {
	osi_Panic("failed to allocate osi_Trace thread-local/cpu-local control structure");
    }
#ifdef OSI_TRACE_BUFFER_CTX_LOCAL
    osi_mem_local_set(_osi_tracepoint_config._tpbuf_key, buf);
#else
    _osi_tracebuf = buf;
#endif

    osi_mem_zero(buf, sizeof(osi_TraceBuffer));


    /*
     * allocate a page-aligned trace buffer
     */
    if (OSI_RESULT_FAIL(osi_mem_page_size_default(&align))) {
	align = 8192;
    }

    len = osi_TraceBuffer_config.buffer_len * sizeof(osi_TracePoint_record);
    buf->buf = osi_mem_aligned_alloc_nosleep(len, align);
    if (osi_compiler_expect_false(buf->buf == osi_NULL)) {
	osi_Panic("failed to allocate osi_Trace thread-local/cpu-local trace buffer");
    }
    osi_mem_zero(buf->buf, len);
    buf->len = len;
    buf->blocks = (len / sizeof(osi_TracePoint_record));
    buf->blocks_mask = buf->blocks - 1;

    /* first buf slot is not valid until an entire rotation due to 
     * cursor startup issues */
    buf->last_idx = 1;

    osi_mutex_Init(&buf->lock,
		   osi_trace_impl_mutex_opts());
    osi_condvar_Init(&buf->cv,
		     osi_trace_impl_condvar_opts());

    if (handle) {
	handle->tracebuf = buf;
    }

    return OSI_OK;
}

osi_result
osi_TraceBuffer_Destroy(osi_TraceBuffer * buf)
{
    if (buf->buf) {
	osi_mem_aligned_free(buf->buf, buf->len);
	buf->buf = osi_NULL;
    }
    osi_mem_aligned_free(buf, sizeof(osi_TraceBuffer));
    return OSI_OK;
}

osi_result
osi_TracBuffer_size_set(osi_size_t len)
{
    return OSI_UNIMPLEMENTED;
}

osi_result
osi_TraceBuffer_size_get(osi_size_t * len_out)
{
    osi_mutex_Lock(&osi_TraceBuffer_config.lock);
    *len_out = osi_TraceBuffer_config.buffer_len;
    osi_mutex_Unlock(&osi_TraceBuffer_config.lock);
    return OSI_OK;
}

OSI_INIT_FUNC_STATIC_DECL(osi_TraceBuffer_config_PkgInit)
{
    osi_result res;
    osi_options_val_t opt;

    osi_mutex_Init(&osi_TraceBuffer_config.lock, osi_NULL);

    /*
     * get the trace buffer size out of the global config data
     */
    res = osi_config_options_Get(OSI_OPTION_TRACE_BUFFER_SIZE,
				 &opt);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "failed to get TRACE_BUFFER_SIZE option value\n");
	goto error;
    }
    osi_TraceBuffer_config.buffer_len = opt.val.v_uint32;

 error:
    return res;
}

OSI_FINI_FUNC_STATIC_DECL(osi_TraceBuffer_config_PkgShutdown)
{
    osi_mutex_Destroy(&osi_TraceBuffer_config.lock);
    return OSI_OK;
}
