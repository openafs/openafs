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
#include <trace/consumer/activation.h>
#include <trace/syscall.h>

/*
 * osi tracing framework
 * trace data consumer library
 * kernel trace framework configuration control
 */

/*
 * this data is supposed to be pulled out of the kernel
 * during process initialization (e.g. osi_PkgInit())
 * therefore we are going to forego the synchronization
 * overhead and just say that if we can't get the data
 * before going multithreaded, oh well
 *
 * multiple threads racing to do the structure update
 * shouldn't actually pose a problem since the operations
 * are completely idempotent
 */
struct osi_trace_consumer_kernel_config_cache {
    osi_uint32 data_valid;
    osi_uint8 key_valid[OSI_TRACE_KERNEL_CONFIG_MAX_ID];
    osi_uint32 value[OSI_TRACE_KERNEL_CONFIG_MAX_ID];
};
osi_static struct osi_trace_consumer_kernel_config_cache osi_trace_consumer_kernel_config_cache;

osi_static osi_result
osi_trace_consumer_kernel_config_get(void)
{
    int i, rv, key;
    osi_result res;
    osi_trace_syscall_config_t cfg;
    osi_trace_syscall_config_element_t el[OSI_TRACE_KERNEL_CONFIG_MAX_ID];

    cfg.nelements = OSI_TRACE_KERNEL_CONFIG_MAX_ID;
    cfg.buf = el;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_CONFIG_GET,
			    (long) &cfg, 
			    0,
			    0,
			    &rv);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    osi_trace_consumer_kernel_config_cache.data_valid = 1;

    for (i = 0; i < cfg.nelements; i++) {
	key = cfg.buf[i].key;
	if ((key > OSI_TRACE_KERNEL_CONFIG_TERMINAL) &&
	    (key < OSI_TRACE_KERNEL_CONFIG_MAX_ID)) {
	    osi_trace_consumer_kernel_config_cache.key_valid[key] = 1;
	    osi_trace_consumer_kernel_config_cache.value[key] = cfg.buf[i].value;
	}
    }

 error:
    return res;
}

osi_result 
osi_trace_consumer_kernel_config_lookup(osi_trace_syscall_config_key_t key,
					osi_uint32 * val_out)
{
    osi_result res = OSI_FAIL;

    if (!osi_trace_consumer_kernel_config_cache.data_valid) {
	res = osi_trace_consumer_kernel_config_get();
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
    }

    if (osi_compiler_expect_true(osi_trace_consumer_kernel_config_cache.key_valid[key])) {
	res = OSI_OK;
	*val_out = osi_trace_consumer_kernel_config_cache.value[key];
    }

 error:
    return res;
}

osi_result
osi_trace_consumer_kernel_config_PkgInit(void)
{
    return osi_trace_consumer_kernel_config_get();
}

osi_result
osi_trace_consumer_kernel_config_PkgShutdown(void)
{
    return OSI_OK;
}
