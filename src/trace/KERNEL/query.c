/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <trace/syscall.h>
#include <osi/osi_kernel.h>
#include <osi/osi_syscall.h>
#include <osi/osi_proc.h>
#include <trace/query.h>
#include <trace/KERNEL/query.h>

/*
 * osi tracing framework
 * reactive monitoring
 * system state query interface
 */

osi_result
osi_trace_query_get(osi_trace_query_id_t id, void * buf, size_t buf_len)
{
    osi_result res = OSI_OK;
    int had_glock, code;

    switch(id) {
    case OSI_TRACE_QUERY_CM_XSTAT:
	if (buf_len < sizeof(CM_XStatCM)) {
	    res = OSI_FAIL;
	    break;
	} else if (buf_len > sizeof(CM_XStatCM)) {
	    buf_len = sizeof(CM_XStatCM);
	}
	had_glock = ISAFS_GLOCK();
	if (osi_compiler_expect_true(!had_glock)) {
	    AFS_GLOCK();
	}
	osi_kernel_copy_out(&CM_XStatCM, buf, buf_len, &code);
	if (osi_compiler_expect_false(code)) {
	    res = OSI_FAIL;
	}
	if (osi_compiler_expect_true(!had_glock)) {
	    AFS_GUNLOCK();
	}
	break;

    default:
	res = OSI_FAIL;
    }

    return res;
}

osi_result
osi_trace_query_PkgInit(void)
{
    return OSI_OK;
}

osi_result
osi_trace_query_PkgShutdown(void)
{
    return OSI_OK;
}
