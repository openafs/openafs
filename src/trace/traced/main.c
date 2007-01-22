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
#include <trace/consumer/cursor.h>
#include <trace/consumer/consumer.h>
#include <trace/syscall.h>
#include <trace/traced/plugin.h>

/*
 * osi tracing framework
 * trace consumer process
 *
 * in environments where system security is a concern, please be sure
 * to run this process without the remote trace rpc interfaces
 * activated.  By default, rtrace is not activated.  In this model,
 * you must provide custom C code to help initialize system state.
 *
 * if you don't feel like rebuilding traceserver, you can LD_PRELOAD
 * your own custom .so.  If you don't mind rebuilding, then just
 * modify the traceserver link line to suit your needs.
 *
 * for more about this, see <src/trace/traced/plugin.h>
 */

osi_options_t * traced_osi_opts = &osi_options_default_daemon;

#include <stdio.h>

int
main(int argc, char ** argv)
{
    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TraceCollector,
					 traced_osi_opts)));
    osi_Assert(OSI_RESULT_OK(osi_Trace_PkgInit()));
    osi_Assert(OSI_RESULT_OK(osi_trace_anly_PkgInit()));
    osi_Assert(OSI_RESULT_OK(osi_traced_plugin_PkgInit()));

    osi_trace_consumer_start();

    while (1) {
	sleep(3600);
    }

    return 0;
}
