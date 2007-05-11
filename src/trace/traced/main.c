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
#include <osi/osi_string.h>
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

osi_options_t traced_osi_opts;

#include <stdio.h>

osi_static osi_result traced_parse_args(int argc, char ** argv);
osi_static void usage(char * prog);

osi_static osi_bool_t Agent = OSI_TRUE;

int
main(int argc, char ** argv)
{
    osi_result res;

    /* copy default daemon options */
    osi_Assert(OSI_RESULT_OK(osi_options_Copy(&traced_osi_opts,
					      &osi_options_default_daemon)));

    /* parse args */
    osi_Assert(OSI_RESULT_OK(traced_parse_args(argc, argv)));

    /* bring up osi, trace consumer, analyzer, agent, and traced plugin subsystems */
    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TraceCollector,
					 &traced_osi_opts)));
    osi_Assert(OSI_RESULT_OK(osi_Trace_PkgInit()));
    osi_Assert(OSI_RESULT_OK(osi_trace_anly_PkgInit()));
    if (Agent == OSI_TRUE) {
	osi_Assert(OSI_RESULT_OK(osi_trace_agent_PkgInit()));
    }
    osi_Assert(OSI_RESULT_OK(osi_traced_plugin_PkgInit()));

    osi_trace_consumer_start();

    while (1) {
	sleep(3600);
    }

    return 0;
}


/*
 * parse command line args
 */
osi_static osi_result 
traced_parse_args(int argc, char ** argv)
{
    int i;
    osi_options_val_t val;

    for (i = 1; i < argc; i++) {
	if (!osi_string_cmp(argv[i], "-help")) {
	    usage(argv[0]);
	    exit(0);
	} else if (!osi_string_cmp(argv[i], "-rxbind")) {
	    val.type = OSI_OPTION_VAL_TYPE_BOOL;
	    val.val.v_bool = OSI_TRUE;
	    osi_options_Set(&traced_osi_opts,
			    OSI_OPTION_RX_BIND,
			    &val);
	} else if (!osi_string_cmp(argv[i], "-noagent")) {
	    Agent = OSI_FALSE;
	} else {
	    usage(argv[0]);
	    exit(1);
	}
    }

    val.val.v_bool = Agent;
    osi_options_Set(&traced_osi_opts,
		    OSI_OPTION_TRACED_LISTEN,
		    &val);

    return OSI_OK;
}

/*
 * usage message
 */
osi_static void
usage(char * prog)
{
    (osi_Msg "usage: %s [-rxbind] [-noagent] [-help]\n",
     (prog != osi_NULL) ? prog : "traceserver");
}
