/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * modular tracepoint table support
 */

#include <osi/osi_impl.h>
#include <osi/osi_kernel.h>
#include <osi/osi_mem.h>
#include <trace/KERNEL/module.h>
#include <trace/generator/module.h>

/* syscall interfaces */
int
osi_trace_module_sys_info(void * info_out)
{
    int code = 0;
    osi_result res;
    osi_trace_generator_info_t info;

    res = osi_trace_module_info(&info);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EAGAIN;
	goto error;
    }

    osi_kernel_copy_out(&info, info_out, sizeof(info), &code);

 error:
    return code;
}

int
osi_trace_module_sys_lookup(long id, void * buf)
{
    int code = 0;
    osi_result res;
    osi_trace_module_info_t * mod;

    mod = (osi_trace_module_info_t *)
	osi_mem_alloc(sizeof(*mod));
    if (osi_compiler_expect_false(mod == osi_NULL)) {
	code = ENOMEM;
	goto error;
    }

    res = osi_trace_module_lookup((osi_trace_module_id_t)id, mod);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto error;
    }

    osi_kernel_copy_out(mod, buf, sizeof(*mod), &code);

 error:
    if (osi_compiler_expect_true(mod != osi_NULL)) {
	osi_mem_free(mod, sizeof(*mod));
    }
    return code;
}

int
osi_trace_module_sys_lookup_by_name(const char * name_in, void * buf)
{
    int code = 0;
    osi_result res;
    osi_trace_module_info_t * mod;
    char * name;
    size_t len;

    mod = (osi_trace_module_info_t *)
	osi_mem_alloc(sizeof(*mod));
    if (osi_compiler_expect_false(mod == osi_NULL)) {
	code = ENOMEM;
	goto error;
    }

    name = (char *) 
	osi_mem_alloc(OSI_TRACE_MODULE_NAME_LEN_MAX);
    if (osi_compiler_expect_false(name == osi_NULL)) {
	code = ENOMEM;
	goto error;
    }
    osi_kernel_handle_copy_in_string(name_in, name, 
				     OSI_TRACE_MODULE_NAME_LEN_MAX,
				     len, code, error);

    res = osi_trace_module_lookup_by_name(name, mod);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto error;
    }

    osi_kernel_copy_out(mod, buf, sizeof(*mod), &code);

 error:
    if (osi_compiler_expect_true(mod != osi_NULL)) {
	osi_mem_free(mod, sizeof(*mod));
    }
    if (osi_compiler_expect_true(name != osi_NULL)) {
	osi_mem_free(name, OSI_TRACE_MODULE_NAME_LEN_MAX);
    }
    return code;
}

int
osi_trace_module_sys_lookup_by_prefix(const char * prefix_in, void * buf)
{
    int code = 0;
    osi_result res;
    osi_trace_module_info_t * mod;
    char * prefix;
    size_t len;

    mod = (osi_trace_module_info_t *)
	osi_mem_alloc(sizeof(*mod));
    if (osi_compiler_expect_false(mod == osi_NULL)) {
	code = ENOMEM;
	goto error;
    }

    prefix = (char *) 
	osi_mem_alloc(OSI_TRACE_MODULE_PREFIX_LEN_MAX);
    if (osi_compiler_expect_false(prefix == osi_NULL)) {
	code = ENOMEM;
	goto error;
    }

    osi_kernel_handle_copy_in_string(prefix_in, prefix, 
				     OSI_TRACE_MODULE_PREFIX_LEN_MAX,
				     len, code, error);

    res = osi_trace_module_lookup_by_prefix(prefix, mod);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto error;
    }

    osi_kernel_copy_out(mod, buf, sizeof(*mod), &code);

 error:
    if (osi_compiler_expect_true(mod != osi_NULL)) {
	osi_mem_free(mod, sizeof(*mod));
    }
    if (osi_compiler_expect_true(prefix != osi_NULL)) {
	osi_mem_free(prefix, OSI_TRACE_MODULE_PREFIX_LEN_MAX);
    }
    return code;
}
