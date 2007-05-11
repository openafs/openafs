/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_thread.h>
#include <osi/osi_cpu.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_condvar.h>
#include <osi/osi_time.h>
#include <trace/consumer/activation.h>
#include <trace/consumer/cursor.h>
#include <trace/consumer/config.h>
#include <trace/syscall.h>
#include <trace/consumer/record_queue.h>

#include <stdlib.h>
#include <stdio.h>

/*
 * osi tracing framework
 * trace consumer
 */

#define OSI_TRACE_CONSUMER_ERROR_THRESHOLD 31 /* must be of the form (2^n)-1 */
#define OSI_TRACE_CONSUMER_TIMEOUT_RESET  120  /* 2 minutes */

osi_result
osi_trace_consumer_loop(void)
{
    int i;
    long buflen;
    osi_result res = OSI_OK;
    osi_TracePoint_record * buf;
    osi_trace_cursor_t cursor;
    osi_TracePoint_rvec_t rv;
    osi_uint32 nread;
    osi_uint32 err;
    osi_time_t now, timeout, last_timeout;

    rv.rvec = osi_NULL;
    rv.nrec = OSI_TRACE_SYSCALL_OP_READV_MAX;
    buflen = rv.nrec * sizeof(osi_TracePoint_record);

    buf = (osi_TracePoint_record *)
	osi_mem_alloc(buflen);

    if (buf == osi_NULL) {
	fprintf(stderr, "failed to allocate trace consumption buffer\n");
	res = OSI_FAIL;
	goto cleanup;
    }

    rv.rvec = (osi_TracePoint_record **)
	osi_mem_alloc(rv.nrec * sizeof(osi_TracePoint_record *));

    if (rv.rvec == osi_NULL) {
	fprintf(stderr, "failed to allocate trace record pointer vector\n");
	res = OSI_FAIL;
	goto cleanup;
    }

    osi_mem_zero(&cursor, sizeof(cursor));

    for (i=0; i < rv.nrec; i++) {
	rv.rvec[i] = &buf[i];
    }

    err = 0;
    timeout = 1;
    last_timeout = 0;
    while (1) {

	/* pull trace records out of the kernel ring buffer */
	res = osi_trace_cursor_readv(&cursor, &rv, 0, &nread);
	if (OSI_RESULT_FAIL(res)) {
	    err++;
	} else {
	    /* enqueue trace records into the internal work queue */
	    for (i = 0; i < nread; i++) {
		osi_trace_consumer_record_queue_enqueue_new(&buf[i]);
	    }
	}
	if ((err & OSI_TRACE_CONSUMER_ERROR_THRESHOLD)
	    == OSI_TRACE_CONSUMER_ERROR_THRESHOLD) {

	    /* prevent livelock by sleeping on repeated failures */
	    fprintf(stderr, "osi_Trace_syscall failing repeatedly; "
		    "consumer thread pausing for a while...\n");
	    osi_time_approx_get(&now, OSI_TIME_APPROX_SAMP_INTERVAL_DEFAULT);
	    if ((now - last_timeout) > OSI_TRACE_CONSUMER_TIMEOUT_RESET) {
		timeout = 1;
	    } else if (!(timeout & 512)) {
		timeout <<= 1;
	    }
	    sleep(timeout);
	    osi_time_approx_get(&last_timeout, 
				OSI_TIME_APPROX_SAMP_INTERVAL_DEFAULT);
	}
    }

 cleanup:
    if (buf)
	osi_mem_free(buf, buflen);
    if (rv.rvec)
	osi_mem_free(rv.rvec, rv.nrec * sizeof(osi_TracePoint_record *));
    return res;
}
