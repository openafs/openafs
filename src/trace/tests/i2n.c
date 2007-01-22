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
#include <trace/consumer/directory.h>
#include <trace/syscall.h>
#include <trace/gen_rgy.h>
#include <trace/directory.h>

/*
 * osi tracing framework
 * trace consumer process
 */

#include <stdio.h>
#include <errno.h>

void
usage(char * prog)
{
    fprintf(stderr, "usage: %s <gen id> <probe id>\n",
	    (prog) ? prog : "");
}

int
main(int argc, char ** argv)
{
    int i, code, ret = 0;
    osi_result res;
    char name[OSI_TRACE_MAX_PROBE_NAME_LEN];
    osi_trace_gen_id_t gen;
    osi_trace_probe_id_t probe;

    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TraceCollector, osi_NULL)));

    if (argc < 3) {
	usage(argv[0]);
	exit(1);
    }

    gen = atoi(argv[1]);
    probe = atoi(argv[2]);

    res = osi_trace_directory_I2N(gen,
				  probe,
				  name,
				  sizeof(name));

    if (OSI_RESULT_FAIL(res)) {
        fprintf(stderr, "call failed\n");
    } else {
        printf("I2N returned: '%s'\n", name);
    }

    return ret;
}
