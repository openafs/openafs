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
#include <osi/osi_kernel.h>
#include <osi/osi_condvar.h>
#include <trace/consumer/activation.h>
#include <trace/consumer/cursor.h>
#include <trace/syscall.h>

/*
 * osi tracing framework
 * trace buffer read cursor
 *
 * trace consumer implementation
 */



/*
 * read next trace buffer element
 */

osi_result
osi_trace_cursor_read(osi_trace_cursor_t * cursor, 
		      osi_TracePoint_record * buf, osi_uint32 flags)
{
    int rv;
    osi_result res;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_READ,
			     (long)cursor, 
			     (long)buf, 
			     (long)flags,
			     &rv);

    if (OSI_RESULT_OK_LIKELY(res)) {
	if (rv == 1) {
	    res = OSI_OK;
	} else if (rv == 0) {
	    res = OSI_TRACE_CONSUMER_RESULT_NO_DATA;
	} else {
	    res = OSI_FAIL;
	}
    }
    return res;
}


/*
 * read next n trace buffer elements
 */

osi_result
osi_trace_cursor_readv(osi_trace_cursor_t * cursor, 
		       osi_TracePoint_rvec_t * rvec,
		       osi_uint32 flags, osi_uint32 * nread)
{
    int rv;
    osi_result res;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_READV,
			     (long)cursor, 
			     (long)rvec, 
			     (long)flags,
			     &rv);

    if (OSI_RESULT_OK_LIKELY(res)) {
	if (rv > 0) {
	    res = OSI_OK;
	    *nread = rv;
	} else if (rv == 0) {
	    res = OSI_TRACE_CONSUMER_RESULT_NO_DATA;
	    *nread = 0;
	} else {
	    res = OSI_FAIL;
	    *nread = 0;
	}
    } else {
	*nread = 0;
    }
    return res;
}
