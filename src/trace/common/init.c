/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi tracing framework
 * common support
 * initialization/shutdown
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <trace/common/init.h>
#include <trace/common/options.h>
#include <trace/gen_rgy.h>
#include <trace/directory.h>
#include <trace/mail.h>
#include <trace/query.h>
#include <trace/syscall.h>

osi_result
osi_trace_common_PkgInit(void)
{
    osi_result res;

    res = osi_trace_common_options_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_common_PkgInit(): common options initialization failed\n");
	goto error;
    }

    res = osi_trace_syscall_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_common_PkgInit(): syscall initialization failed\n");
	goto error;
    }

    res = osi_trace_activation_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_common_PkgInit(): activation initialization failed\n");
	goto error;
    }

    res = osi_trace_gen_rgy_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_common_PkgInit(): gen_rgy initialization failed\n");
	goto no_gen;
    }

    res = osi_trace_mail_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_common_PkgInit(): mail initialization failed\n");
	goto no_gen;
    }

 no_gen:
    res = osi_trace_directory_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_common_PkgInit(): directory initialization failed\n");
	goto error;
    }

    /* XXX we will eventually want this in userspace as well... */
#if defined(OSI_KERNELSPACE_ENV)
    res = osi_trace_query_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "osi_trace_common_PkgInit(): query initialization failed\n");
	goto error;
    }
#endif /* OSI_KERNELSPACE_ENV */

 error:
    return res;
}

osi_result
osi_trace_common_PkgShutdown(void)
{
    osi_result res;

    /* XXX we will eventually want this in userspace as well... */
#if defined(OSI_KERNELSPACE_ENV)
    res = osi_trace_query_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }
#endif /* OSI_KERNELSPACE_ENV */

    res = osi_trace_directory_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_mail_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_gen_rgy_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_activation_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_syscall_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_common_options_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

 error:
    return res;
}
