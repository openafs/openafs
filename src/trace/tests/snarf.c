/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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
    osi_options_t opts;
    osi_options_val_t val;

    osi_Assert(OSI_RESULT_OK(osi_options_Init(osi_ProgramType_TraceCollector, &opts)));
    val.type = OSI_OPTION_VAL_TYPE_BOOL;
    val.val.v_bool = OSI_FALSE;
    osi_Assert(OSI_RESULT_OK(osi_options_Set(&opts,
					     OSI_OPTION_TRACE_GEN_RGY_REGISTRATION,
					     &val)));
    osi_Assert(OSI_RESULT_OK(osi_options_Set(&opts,
					     OSI_OPTION_TRACE_CONSUMER_START_I2N_THREAD,
					     &val)));
    osi_Assert(OSI_RESULT_OK(osi_options_Set(&opts,
					     OSI_OPTION_TRACE_START_MAIL_THREAD,
					     &val)));

    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TraceCollector, &opts)));
    osi_Assert(OSI_RESULT_OK(osi_options_Destroy(&opts)));
    osi_Assert(OSI_RESULT_OK(osi_Trace_PkgInit()));

    count = atoi(argv[1]);
    osi_mem_zero(&cursor, sizeof(cursor));

    for (i=0; i < count; i++) {
	osi_int64 arg_val;
	afs_int64 arg_val_afs;
	osi_timeval32_t tv;
	osi_uint32 j;
	int hi;
	unsigned int lo;

	fprintf(stderr, "calling osi_trace_cursor_read...");
        res = osi_trace_cursor_read(&cursor, &rec, 0);
	fprintf(stderr, "done\n");
	if (OSI_RESULT_FAIL(res)) {
		fprintf(stderr, "call failed\n");
	} else {
		fprintf(stderr, "call succeeded\n");
		printf("== timestamp: {%d, %d}\n", rec.timestamp.tv_sec, rec.timestamp.tv_nsec);
		printf("== version: %u, gen_id: %u, pid: %d, tid: %d\n",
			rec.version, rec.gen_id, rec.pid, rec.tid);
		printf("== tags: {%u, %u}, probe: %u, nargs: %u\n",
			rec.tags[0], rec.tags[1], rec.probe, rec.nargs);
		printf("== args: ");
		for (j = 0; j < rec.nargs; j++) {
		    res = osi_TracePoint_record_arg_get64(&rec,
							  j,
							  &arg_val);
		    if (OSI_RESULT_FAIL(res)) {
			fprintf(stderr, "arg_get64 failed for argument %u\n", j);
		    } else {
			osi_convert_osi64_to_afs64(arg_val, arg_val_afs);
			SplitInt64(arg_val_afs, hi, lo);
			printf("{%d, %u}, ", hi, lo);
		    }
		}
		printf("\n");
	}
    }

    return ret;
}
