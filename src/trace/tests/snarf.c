/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi.h>
#include <osi/osi_trace.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_condvar.h>
#include <trace/consumer/consumer.h>
#include <trace/consumer/cursor.h>
#include <trace/syscall.h>

/*
 * osi tracing framework
 * trace consumer process
 */

#include <stdio.h>
#include <errno.h>

int
main(int argc, char ** argv)
{
    int i, code, count, ret = 0;
    osi_TracePoint_record rec;
    osi_trace_cursor_t cursor;
    osi_result res;

    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TraceCollector, osi_NULL)));
    osi_Assert(OSI_RESULT_OK(osi_Trace_PkgInit()));

    count = atoi(argv[1]);
    osi_mem_zero(&cursor, sizeof(cursor));

    for (i=0; i < count; i++) {
	fprintf(stderr, "calling osi_trace_cursor_read...");
        res = osi_trace_cursor_read(&cursor, &rec, 0);
	fprintf(stderr, "done\n");
	if (OSI_RESULT_FAIL(res)) {
		fprintf(stderr, "call failed\n");
	} else {
		fprintf(stderr, "call succeeded\n");
	}
    }

    return ret;
}
