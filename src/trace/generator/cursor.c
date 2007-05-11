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
#include <osi/osi_mem_local.h>
#include <osi/osi_kernel.h>
#include <osi/osi_condvar.h>
#include <trace/generator/activation.h>
#include <trace/generator/cursor.h>
#include <trace/generator/generator.h>
#include <trace/syscall.h>

/*
 * osi tracing framework
 * trace buffer read cursor
 *
 * trace generator implementation
 */

/* only turn on cursors for buffer commit method types */
#if OSI_TRACE_COMMIT_METHOD_IS(OSI_TRACE_COMMIT_METHOD_BUFFER)


#if defined(OSI_TRACE_BUFFER_CTX_LOCAL)
osi_static osi_inline osi_result
osi_trace_cursor_wait(osi_TraceBuffer * tracebuf, osi_register_uint idx)
{
    int code;
    do {
#if defined(OSI_IMPLEMENTS_CONDVAR_WAIT_SIG)
	code = osi_condvar_WaitSig(&tracebuf->cv, &tracebuf->lock);
	if (code) {
	    return OSI_FAIL;
	}
#else /* !OSI_IMPLEMENTS_CONDVAR_WAIT_SIG */
	osi_condvar_Wait(&tracebuf->cv, &tracebuf->lock);
#endif /* !OSI_IMPLEMENTS_CONDVAR_WAIT_SIG */
    } while (tracebuf->last_idx == idx);
    return OSI_OK;
}
#else /* !OSI_TRACE_BUFFER_CTX_LOCAL */
osi_static osi_inline osi_result
osi_trace_cursor_wait(osi_TraceBuffer * tracebuf, osi_register_uint idx)
{
    int code;
    while (tracebuf->last_valid == idx) {
#if defined(OSI_IMPLEMENTS_CONDVAR_WAIT_SIG)
	code = osi_condvar_WaitSig(&tracebuf->cv, &tracebuf->lock);
	if (code) {
	    return OSI_FAIL;
	}
#else /* !OSI_IMPLEMENTS_CONDVAR_WAIT_SIG */
	osi_condvar_Wait(&tracebuf->cv, &tracebuf->lock);
#endif /* !OSI_IMPLEMENTS_CONDVAR_WAIT_SIG */
    }
    return OSI_OK;
}
#endif /* !OSI_TRACE_BUFFER_CTX_LOCAL */


/*
 * read next trace buffer element
 */

#ifdef OSI_TRACE_BUFFER_CTX_LOCAL 

osi_result
osi_trace_cursor_read(osi_trace_cursor_t * cursor, osi_TracePoint_record * buf)
{
#if (OSI_TYPE_REGISTER_BITS == 32)
    osi_register_uint hi, idx;
#else
    osi_register_uint idx;
#endif
    int reset_cursor = 0, copy_res = 0;
    osi_result res = OSI_OK;
    osi_TraceBuffer * tracebuf;

   tracebuf = (osi_TraceBuffer *) osi_mem_local_get(_osi_tracepoint_config._tpbuf_key);

#if (OSI_TYPE_REGISTER_BITS == 32)
    SplitInt64(cursor->last_idx, hi, idx);
#else
    idx = cursor->last_idx;
#endif

    /* deal with wrap-around and bad inputs */
    if (idx > tracebuf->last_idx) {
	idx = tracebuf->last_idx;
	reset_cursor = 1;
    }

    /* make sure there's stuff to return */
    if (tracebuf->last_idx == idx) {
	osi_mutex_Lock(&tracebuf->lock);
	res = osi_trace_cursor_wait(tracebuf, idx);
	osi_mutex_Unlock(&tracebuf->lock);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto done;
	}
    }

    /* check to see if we've fallen too far behind */
    if ((tracebuf->last_idx - idx) > tracebuf->blocks) {
	cursor->records_lost = tracebuf->last_idx - idx - tracebuf->blocks;
	idx = tracebuf->last_idx + 1;
    } else {
	cursor->records_lost = 0;
    }

    /* copy out record */
    osi_kernel_copy_out(&tracebuf->buf[idx & tracebuf->blocks_mask], 
			buf, 
			sizeof(osi_TracePoint_record), 
			&copy_res);
    if (osi_compiler_expect_false(copy_res)) {
	res = OSI_FAIL;
    }

    /* update cursor */
#if defined(OSI_ENV_NATIVE_INT64_TYPE)
    if (reset_cursor) {
	cursor->last_idx = idx + 1;
    } else {
	cursor->last_idx++;
    }
#else
    if (reset_cursor) {
	FillInt64(cursor->last_idx, hi+1, idx+1);
    } else {
	IncUInt64(&cursor->last_idx);
    }
#endif

 done:
    osi_mem_local_put(_osi_tracepoint_config._tpbuf_key);
    return res;
}

#else /* !OSI_TRACE_BUFFER_CTX_LOCAL */

osi_result
osi_trace_cursor_read(osi_trace_cursor_t * cursor, osi_TracePoint_record * buf)
{
    osi_register_uint idx, temp;
#if (OSI_TYPE_REGISTER_BITS == 32)
    osi_register_uint hi;
#endif
    int copy_res = 0;
    osi_result res = OSI_OK;
    osi_TraceBuffer * tracebuf;

    tracebuf = _osi_tracebuf;
    osi_mutex_Lock(&tracebuf->lock);

#if (OSI_TYPE_REGISTER_BITS == 32)
    SplitInt64(cursor->last_idx, hi, idx);
#else
    idx = cursor->last_idx;
#endif

    /* deal with wrap-around and bad inputs */
    if (idx > tracebuf->last_valid) {
	if (tracebuf->last_idx > tracebuf->blocks) {
	    idx = tracebuf->last_idx - tracebuf->blocks;
	} else {
	    idx = 0;
	}
    }

    /* make sure there's stuff to return */
    res = osi_trace_cursor_wait(tracebuf, idx);
    if (OSI_RESULT_FAIL(res)) {
	goto done;
    }

    /* 
     * check to see if we've fallen too far behind
     * also set the records lost count for this call
     */
    if ((tracebuf->last_idx - idx) > tracebuf->blocks) {
	cursor->records_lost = tracebuf->last_valid - idx - 
	    (tracebuf->blocks - (tracebuf->last_idx - tracebuf->last_valid));
	idx = tracebuf->last_idx - tracebuf->blocks - 1;
    } else {
	cursor->records_lost = 0;
    }

    /* now increment to get next record */
    idx++;

    /* copy out record */
    osi_kernel_copy_out(&tracebuf->buf[idx & tracebuf->blocks_mask], 
			buf, 
			sizeof(osi_TracePoint_record), 
			&copy_res);
    if (osi_compiler_expect_false(copy_res)) {
	res = OSI_FAIL;
	goto done;
    }

    /* update cursor */
#if defined(OSI_ENV_NATIVE_INT64_TYPE)
    cursor->last_idx = idx;
#else
    FillInt64(cursor->last_idx, hi+1, idx);
#endif

 done:
    osi_mutex_Unlock(&tracebuf->lock);
    return res;
}

#endif /* !OSI_TRACE_BUFFER_CTX_LOCAL */



/*
 * read next n trace buffer elements
 */

#ifdef OSI_TRACE_BUFFER_CTX_LOCAL

osi_result
osi_trace_cursor_readv(osi_trace_cursor_t * cursor, 
		       osi_TracePoint_rvec_t * rv, 
		       osi_uint32 flags, osi_uint32 * nread)
{
#if (OSI_TYPE_REGISTER_BITS == 32)
    osi_register_uint hi, idx;
#else
    osi_register_uint idx;
#endif
    osi_uint64 idx_delta;
    int i, reset_cursor = 0, copy_res = 0;
    osi_result res = OSI_OK;
    osi_TraceBuffer * tracebuf;
    osi_register_uint tot_xfer, gap;

   tracebuf = (osi_TraceBuffer *) osi_mem_local_get(_osi_tracepoint_config._tpbuf_key);

#if (OSI_TYPE_REGISTER_BITS == 32)
    SplitInt64(cursor->last_idx, hi, idx);
#else
    idx = cursor->last_idx;
#endif

    /* deal with wrapping and bad inputs */
    if (idx > tracebuf->last_idx) {
	idx = tracebuf->last_idx;
	reset_cursor = 1;
    }

    /* make sure there's stuff to return */
    if (tracebuf->last_idx == idx) {
	if (flags & OSI_TRACE_SYSCALL_OP_READV_FLAG_NOWAIT) {
	    res = OSI_FAIL;
	    goto cleanup;
	}
	osi_mutex_Lock(&tracebuf->lock);
	res = osi_trace_cursor_wait(tracebuf, idx);
	osi_mutex_Unlock(&tracebuf->lock);
	if (OSI_RESULT_FAIL(res)) {
	    goto cleanup;
	}
    }

    /* check to see if we've fallen too far behind */
    if ((tracebuf->last_idx - idx) > tracebuf->blocks) {
	cursor->records_lost = tracebuf->last_idx - idx - tracebuf->blocks;
	idx = tracebuf->last_idx - tracebuf->blocks + 1;
    } else {
	cursor->records_lost = 0;
    }

    /* calculate the total number of records to return */
    gap = tracebuf->last_idx - idx;
    tot_xfer = (osi_register_uint) rv->nrec;
    if (tot_xfer > gap) {
	tot_xfer = gap;
    }

    /* copyout the tracepoint records */
    for (i = 0; i < tot_xfer; i++, idx++) {
#if defined(OSI_ENV_KERNELSPACE)
	AFS_COPYOUT(&tracebuf->buf[idx & tracebuf->blocks_mask],
		    rv->rvec[i], sizeof(osi_TracePoint_record), copy_res);
	if (osi_compiler_expect_false(copy_res)) {
	    goto done;
	}
#else
	memcpy(rv->rvec[i], &tracebuf->buf[idx & tracebuf->blocks_mask],
	       sizeof(osi_TracePoint_record));
#endif
    }

    /* update cursor */
 done:
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
    if (reset_cursor) {
	cursor->last_idx = idx;
    } else {
	cursor->last_idx += tot_xfer;
    }
#else
    if (reset_cursor) {
	FillInt64(cursor->last_idx, hi+1, idx);
    } else {
	FillInt64(idx_delta, 0, tot_xfer);
	AddUInt64(cursor->last_idx, idx_delta, &cursor->last_idx);
    }
#endif

    *nread = (osi_uint32) tot_xfer;

 cleanup:
    osi_mem_local_put(_osi_tracepoint_config._tpbuf_key);
    return res;
}

#else /* !OSI_TRACE_BUFFER_CTX_LOCAL */

osi_result
osi_trace_cursor_readv(osi_trace_cursor_t * cursor,
		       osi_TracePoint_rvec_t * rv,
		       osi_uint32 flags, osi_uint32 * nread)
{
#if (OSI_TYPE_REGISTER_BITS == 32)
    osi_register_uint hi, idx;
#else
    osi_register_uint idx;
#endif
    osi_uint64 idx_delta;
    int i, reset_cursor = 0, copy_res = 0;
    osi_result res = OSI_OK;
    osi_TraceBuffer * tracebuf;
    osi_register_uint tot_xfer, gap;

    tracebuf = _osi_tracebuf;
    osi_mutex_Lock(&tracebuf->lock);

#if (OSI_TYPE_REGISTER_BITS == 32)
    SplitInt64(cursor->last_idx, hi, idx);
#else
    idx = cursor->last_idx;
#endif

    /* deal with wrap-around and bad inputs */
    if (idx > tracebuf->last_valid) {
	idx = tracebuf->last_valid;
	reset_cursor = 1;
    }

    /* make sure there's stuff to return */
    if (tracebuf->last_valid == idx) {
	if (flags & OSI_TRACE_SYSCALL_OP_READV_FLAG_NOWAIT) {
	    res = OSI_FAIL;
	    goto cleanup;
	}
	res = osi_trace_cursor_wait(tracebuf, idx);
	if (OSI_RESULT_FAIL(res)) {
	    goto cleanup;
	}
    }

    /* check to see if we've fallen too far behind */
    if ((tracebuf->last_idx - idx) > tracebuf->blocks) {
	idx = tracebuf->last_idx;
    }

    /* calculate the total number of records to return */
    gap = tracebuf->last_valid - idx;
    tot_xfer = (osi_register_uint) rv->nrec;
    if (tot_xfer > gap) {
	tot_xfer = gap;
    }

    for (i = 0; i < tot_xfer; i++, idx++) {
#if defined(OSI_ENV_KERNELSPACE)
	AFS_COPYOUT(&tracebuf->buf[idx & tracebuf->blocks_mask],
		    rv->rvec[i], sizeof(osi_TracePoint_record), copy_res);
	if (osi_compiler_expect_false(copy_res)) {
	    goto done;
	}
#else
	memcpy(rv->rvec[i], &tracebuf->buf[idx & tracebuf->blocks_mask],
	       sizeof(osi_TracePoint_record));
#endif
    }

    /* update cursor */
 done:
#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
    if (reset_cursor) {
	cursor->last_idx = idx;
    } else {
	cursor->last_idx += tot_xfer;
    }
#else
    if (reset_cursor) {
	FillInt64(cursor->last_idx, hi+1, idx);
    } else {
	FillInt64(idx_delta, 0, tot_xfer);
	AddUInt64(cursor->last_idx, idx_delta, &cursor->last_idx);
    }
#endif

    *nread = (osi_uint32) tot_xfer;

 cleanup:
    osi_mutex_Unlock(&tracebuf->lock);
    return res;
}

#endif /* !OSI_TRACE_BUFFER_CTX_LOCAL */

#endif /* OSI_TRACE_COMMIT_METHOD_IS(OSI_TRACE_COMMIT_METHOD_BUFFER) */
