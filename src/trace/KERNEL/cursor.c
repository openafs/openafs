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
#include <trace/KERNEL/config.h>
#include <trace/KERNEL/cursor.h>
#include <trace/KERNEL/gen_rgy.h>
#include <trace/KERNEL/postmaster.h>
#include <trace/generator/module.h>

/*
 * osi tracing framework
 * trace syscall
 */

int
osi_trace_cursor_sys_read(long p1, long p2, long p3, long * retVal)
{
    osi_trace_cursor_t cursor;
    osi_result res;
    int code;

    osi_kernel_copy_in(p1, &cursor, sizeof(cursor), &code);
    if (osi_compiler_expect_true(!code)) {
	res = osi_trace_cursor_read(&cursor, (osi_TracePoint_record *) p2);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = EAGAIN;
	} else {
	    *retVal = 1;
	    osi_kernel_copy_out(&cursor, p1, sizeof(cursor), &code);
	}
    }

    return code;
}


int
osi_trace_cursor_sys_readv(long p1, long p2, long p3, long * retVal)
{
    osi_trace_cursor_t cursor;
    osi_TracePoint_rvec_t rv;
    osi_TracePoint_rvec_t rv_u;

    int code = 0;
    osi_result res;

    rv.nrec = 0;
    rv.rvec = osi_NULL;

    if (osi_compiler_expect_false(p3 < 0)) {
	code = EINVAL;
	goto done;
    }

    osi_kernel_copy_in(p1, &cursor, sizeof(cursor), &code);
    if (osi_compiler_expect_false(code)) {
	goto done;
    }

    osi_kernel_copy_in((char *) p2, (caddr_t) &rv_u, sizeof(rv_u), &code);
    if (osi_compiler_expect_false(code)) {
	goto done;
    }

    if (osi_compiler_expect_false(!rv_u.nrec || rv_u.nrec > OSI_TRACE_SYSCALL_OP_READV_MAX)) {
	code = EINVAL;
	goto done;
    }

    rv.nrec = rv_u.nrec;
    rv.rvec = (osi_TracePoint_record **) osi_mem_alloc(rv.nrec * sizeof(osi_TracePoint_record *));
    if (osi_compiler_expect_false(rv.rvec == osi_NULL)) {
	code = EAGAIN;
	goto done;
    }

    osi_kernel_copy_in(rv_u.rvec, (caddr_t) rv.rvec, rv.nrec * sizeof(osi_TracePoint_record *), &code);
    if (osi_compiler_expect_false(code)) {
	osi_mem_free(rv.rvec, rv.nrec * sizeof(osi_TracePoint_record *));
	goto done;
    }

    {
	osi_uint32 nrec;
	res = osi_trace_cursor_readv(&cursor, &rv, (osi_uint32) p3, &nrec);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = EAGAIN;
	} else {
	    *retVal = (long) nrec;
	    osi_kernel_copy_out(&cursor, p1, sizeof(cursor), &code);
	}
    }

    if (osi_compiler_expect_true(rv.rvec != osi_NULL)) {
	osi_mem_free(rv.rvec, rv.nrec * sizeof(osi_TracePoint_record *));
    }

 done:
    return code;
}

#if !OSI_DATAMODEL_IS(OSI_ILP32_ENV)
int
osi_trace_cursor_sys32_readv(long p1, long p2, long p3, long * retVal)
{
    osi_trace_cursor_t cursor;
    osi_TracePoint_rvec_t rv;
    int code = 0;
    osi_result res;
    osi_TracePoint_rvec32_t rv_u;
    osi_uint32 * buf32;
    int i;

    rv.nrec = 0;
    rv.rvec = osi_NULL;

    if (osi_compiler_expect_false(p3 < 0)) {
	code = EINVAL;
	goto done;
    }

    osi_kernel_copy_in(p1, &cursor, sizeof(cursor), &code);
    if (osi_compiler_expect_false(code)) {
	goto done;
    }

    osi_kernel_copy_in(p2, &rv_u, sizeof(rv_u), &code);
    if (osi_compiler_expect_false(code)) {
	goto done;
    }

    if (osi_compiler_expect_false(!rv_u.nrec || rv_u.nrec > OSI_TRACE_SYSCALL_OP_READV_MAX)) {
	code = EINVAL;
	goto done;
    }

    rv.nrec = rv_u.nrec;
    rv.rvec = (osi_TracePoint_record **) osi_mem_alloc(rv.nrec * sizeof(osi_TracePoint_record *));
    if (osi_compiler_expect_false(rv.rvec == osi_NULL)) {
	code = EAGAIN;
	goto done;
    }

    buf32 = (osi_uint32 *) osi_mem_alloc(rv.nrec * sizeof(osi_uint32));
    if (osi_compiler_expect_false(buf32 == osi_NULL)) {
	osi_mem_free(rv.rvec, rv.nrec * sizeof(osi_TracePoint_record *));
	code = EAGAIN;
	goto done;
    }

    osi_kernel_copy_in(rv_u.rvec, buf32, rv.nrec * sizeof(osi_uint32), &code);
    if (osi_compiler_expect_false(code)) {
	osi_mem_free(rv.rvec, rv.nrec * sizeof(osi_TracePoint_record *));
	osi_mem_free(buf32, rv.nrec * sizeof(osi_uint32));
	goto done;
    }

    for (i=0; i < rv.nrec; i++) {
	rv.rvec[i] = (osi_TracePoint_record *) buf32[i];
    }

    osi_mem_free(buf32, rv.nrec * sizeof(osi_uint32));

    {
	osi_uint32 nrec;
	res = osi_trace_cursor_readv(&cursor, &rv, (osi_uint32) p3, &nrec);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = EAGAIN;
	} else {
	    *retVal = (long) nrec;
	    osi_kernel_copy_out(&cursor, p1, sizeof(cursor), &code);
	}
    }

    if (osi_compiler_expect_true(rv.rvec != osi_NULL)) {
	osi_mem_free(rv.rvec, rv.nrec * sizeof(osi_TracePoint_record *));
    }

 done:
    return code;
}
#endif /* !OSI_DATAMODEL_IS(OSI_ILP32_ENV) */
