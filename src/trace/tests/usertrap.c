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
#include <trace/generator/generator.h>
#include <trace/syscall.h>
#include <trace/event/event.h>

/*
 * osi tracing framework
 * trace consumer process
 */

#include <stdio.h>
#include <errno.h>

/* for non-variadic macro environments we redefine osi_TraceFunc_Event to 
 * osi_TraceFunc_VarEvent and do static inline variadic functions for
 * each tracepoint.
 *
 * obviously we don't want that behavior here
 */
#undef osi_TraceFunc_Event

int
main(int argc, char ** argv)
{
    int i, code, count, ret = 0;
    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_Fileserver, osi_NULL)));

    if (argc < 3) {
	fprintf(stderr, "usage: %s <trap count> <probe id>\n", (argc) ? argv[0] : "usertrap");
    }

    count = atoi(argv[1]);
    for (i = 0; i < count; i++) {
        osi_TraceFunc_Event(atoi(argv[2]), osi_Trace_Event_FSEvent_Id, 4, i, argv, argv[0], argv[1]);
    }

    return ret;
}
