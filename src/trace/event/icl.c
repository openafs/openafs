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
#include <trace/gen_rgy.h>
#include <trace/event/event.h>
#include <trace/generator/generator.h>


/* for non-variadic macro environments we redefine osi_TraceFunc_iclEvent to 
 * osi_TraceFunc_iclVarEvent and do static inline variadic functions for
 * each tracepoint.
 *
 * just in case someone hard codes things differently, let's define
 * both interfaces for all cases */
#undef osi_TraceFunc_iclEvent


typedef union {
    osi_register_int int_arg;
    void * ptr_arg;
} osi_icl_arg_t;

osi_result
osi_TraceFunc_iclEvent(osi_trace_probe_id_t id, osi_Trace_EventType type, int nf, ...)
{
    osi_result res;
    va_list args;

    va_start(args, nf);
    res = osi_TraceFunc_iclVarEvent(id, type, nf, args);
    va_end(args);

    return res;
}

osi_static osi_result
osi_icl_AddPayload(osi_Trace_EventHandle * handle, 
		   int type, osi_icl_arg_t arg)
{
    osi_result res = OSI_OK;
    osi_int64 tmp64;

    switch (type) {
    case ICL_TYPE_HYPER:
	osi_convert_hyper_to_osiU64(*((afs_hyper_t *)arg.ptr_arg), tmp64);
	res = osi_TracePoint_record_arg_add64(handle->data, tmp64);
	break;

    case ICL_TYPE_INT64:
	osi_convert_afs64_to_osi64(*((afs_int64 *)arg.ptr_arg), tmp64);
	res = osi_TracePoint_record_arg_add64(handle->data, tmp64);
	break;

    case ICL_TYPE_INT32:
    case ICL_TYPE_LONG:
	res = osi_TracePoint_record_arg_add(handle->data, arg.int_arg);
	break;

    case ICL_TYPE_POINTER:
#if OSI_DATAMODEL_IS(OSI_LP64_ENV) || OSI_DATAMODEL_IS(OSI_P64_ENV)
	res = osi_TracePoint_record_arg_add64(handle->data, (osi_int64)(arg.ptr_arg));
#else
	res = osi_TracePoint_record_arg_add(handle->data, (osi_register_int)(arg.ptr_arg));
#endif
	break;

    case ICL_TYPE_VENUS_FID:
	(void)osi_TracePoint_record_arg_add(handle->data,
					    (osi_register_int)((struct VenusFid *)arg.ptr_arg)->Cell);
	(void)osi_TracePoint_record_arg_add(handle->data,
					    (osi_register_int)((struct VenusFid *)arg.ptr_arg)->Fid.Volume);
	(void)osi_TracePoint_record_arg_add(handle->data,
					    (osi_register_int)((struct VenusFid *)arg.ptr_arg)->Fid.Vnode);
	res = osi_TracePoint_record_arg_add(handle->data,
					    (osi_register_int)((struct VenusFid *)arg.ptr_arg)->Fid.Unique);
	break;

    case ICL_TYPE_VICE_FID:
	(void)osi_TracePoint_record_arg_add(handle->data,
					    (osi_register_int)((struct AFSFid *)arg.ptr_arg)->Volume);
	(void)osi_TracePoint_record_arg_add(handle->data,
					    (osi_register_int)((struct AFSFid *)arg.ptr_arg)->Vnode);
	res = osi_TracePoint_record_arg_add(handle->data,
					    (osi_register_int)((struct AFSFid *)arg.ptr_arg)->Unique);
	break;

#if !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN80_ENV)
    case ICL_TYPE_OS_FID:
	/* XXX OS fid tracing is also quite difficult since our userspace
	 * consumers need to understand the OS fid structure.  let's just
	 * skip this one for now...
	 */
	break;
#endif

    case ICL_TYPE_FID:
	/* XXX fid tracing is extremely broken 
	 * (read: the old icl fid trace code was dangerous)
	 *
	 * we now differentiate between os, venus, and vice fids
	 */
	break;

    case ICL_TYPE_OFFSET:
	/* XXX offset tracing looks rather broken right now;
	 * we pass in afs_size_t's, afs_fsize_t's, afs_foff_t's and
	 * act as if they're all identical */
	break;

    case ICL_TYPE_STRING:
	/* XXX dumping strings is annoying; skip for now */
	break;

    case ICL_TYPE_NONE:
	res = OSI_FAIL;
	break;
    }

    return res;
}

osi_result
osi_TraceFunc_iclVarEvent(osi_trace_probe_id_t id,
			  osi_Trace_EventType type,
			  int nf,
			  va_list args)
{
    osi_result res = OSI_OK;
    osi_Trace_EventHandle handle;
    int i;
    osi_register_int types;
    osi_icl_arg_t data;

    osi_TraceFunc_Prologue();
    osi_TraceBuffer_Allocate(&handle);

    handle.data->probe = id;
    handle.data->tags[0] = (osi_uint8) type;

    if (osi_compiler_expect_true(nf > 0)) {
	types = va_arg(args, osi_register_int);
	res = osi_TracePoint_record_arg_add(handle.data, types);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}

	for (i=1; i < nf; i++) {
	    data.int_arg = va_arg(args, osi_register_int);
	    res = osi_icl_AddPayload(&handle, types & 0x3f, data);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		break;
	    }
	    types >>= 6;
	}
    }

    osi_TraceBuffer_Stamp(&handle);
    osi_TraceBuffer_Commit(&handle);

 done:
    osi_TraceFunc_Epilogue();
    return res;

 error:
    osi_TraceBuffer_Rollback(&handle);
    goto done;
}
