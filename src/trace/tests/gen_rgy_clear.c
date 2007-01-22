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
#include <trace/gen_rgy.h>
#include <trace/USERSPACE/gen_rgy.h>
#include <trace/consumer/gen_rgy.h>
#include <trace/consumer/consumer.h>
#include <trace/consumer/config.h>
#include <trace/syscall.h>

/*
 * osi tracing framework
 * trace consumer process
 */

#include <stdio.h>
#include <errno.h>

osi_static void
unregister(osi_trace_gen_id_t i)
{
    osi_result res;

    (osi_Msg "calling osi_trace_gen_rgy_unregister(%d)...", i);
    res = osi_trace_gen_rgy_unregister(i);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "FAILED!!!\n");
    } else {
	(osi_Msg "done\n");
    }
}

int
main(int argc, char ** argv)
{
    int ret = 0;
    osi_result res;
    osi_uint32 max_id;
    osi_trace_gen_id_t i;
    osi_options_t opts;
    osi_options_val_t val;

    osi_Assert(OSI_RESULT_OK(osi_options_Init(osi_ProgramType_TraceCollector, &opts)));
    val.type = OSI_OPTION_VAL_TYPE_BOOL;
    val.val.v_bool = OSI_FALSE;
    osi_Assert(OSI_RESULT_OK(osi_options_Set(&opts,
					     OSI_OPTION_TRACE_GEN_RGY_REGISTRATION,
					     &val)));
    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TraceCollector, &opts)));
    osi_Assert(OSI_RESULT_OK(osi_Trace_PkgInit()));

    res = osi_trace_consumer_kernel_config_lookup(OSI_TRACE_KERNEL_CONFIG_GEN_RGY_MAX_ID,
						  &max_id);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "%s: failed to get gen_rgy max id out of trace kernel config data\n", 
	 argv[0]);
	ret = -1;
	goto error;
    }

    if (argc < 2) {
	for (i=1; i <= max_id; i++) {
	    unregister(i);
	}
    } else {
	i = atoi(argv[1]);
	unregister(i);
    }

 error:
    osi_Assert(OSI_RESULT_OK(osi_Trace_PkgShutdown()));
    osi_Assert(OSI_RESULT_OK(osi_PkgShutdown()));

    return ret;
}
