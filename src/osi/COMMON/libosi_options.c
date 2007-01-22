/*
 * Copyright 2006, Sine Nomine and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_mem.h>

/* type of every osi_options_t parameter
 * indexed by osi_options_param_t enumeration value */
osi_options_val_type_t osi_option_types[OSI_OPTION_MAX_ID+1] =
    {
	OSI_OPTION_VAL_TYPE_INVALID,    /* MIN_ID */
	OSI_OPTION_VAL_TYPE_BOOL,       /* detect_cpu */
	OSI_OPTION_VAL_TYPE_BOOL,       /* detect_cache */
	OSI_OPTION_VAL_TYPE_BOOL,       /* start_bkg_threads */
	OSI_OPTION_VAL_TYPE_BOOL,       /* init_syscall */
	OSI_OPTION_VAL_TYPE_BOOL,       /* ioctl_syscall_persistent_fd */
	OSI_OPTION_VAL_TYPE_BOOL,       /* trace_init_syscall */
	OSI_OPTION_VAL_TYPE_BOOL,       /* trace_gen_rgy_registration */
	OSI_OPTION_VAL_TYPE_BOOL,       /* trace_start_mail_thread */
	OSI_OPTION_VAL_TYPE_UINT32,     /* trace_buffer_size */
	OSI_OPTION_VAL_TYPE_BOOL,       /* traced_listen */
	OSI_OPTION_VAL_TYPE_UINT16,     /* traced_port */
	OSI_OPTION_VAL_TYPE_INVALID /*end */
    };

/* default options for in-kernel libosi */
osi_options_t osi_options_default_kernel =
    {
	OSI_TRUE,    /* detect_cpu */
	OSI_TRUE,    /* detect_cache */
	OSI_TRUE,    /* start_bkg_threads */
	OSI_TRUE,    /* init_syscall */
	OSI_FALSE,   /* ioctl_syscall_persistent_fd */
	OSI_TRUE,    /* trace_init_syscall */
	OSI_FALSE,   /* trace_gen_rgy_registration */
	OSI_FALSE,   /* trace_start_mail_thread */
	128,         /* trace_buffer_size */
	OSI_FALSE,   /* traced_listen */
	0,           /* traced_port */
	0 /* end */
    };

/* default options for ukernel (no trace for now) */
osi_options_t osi_options_default_ukernel =
    {
	OSI_TRUE,    /* detect_cpu */
	OSI_TRUE,    /* detect_cache */
	OSI_FALSE,   /* start_bkg_threads */
	OSI_FALSE,   /* init_syscall */
	OSI_FALSE,   /* ioctl_syscall_persistent_fd */
	OSI_FALSE,   /* trace_init_syscall */
	OSI_FALSE,   /* trace_gen_rgy_registration */
	OSI_FALSE,   /* trace_start_mail_thread */
	128,         /* trace buffer_size */
	OSI_FALSE,   /* traced_listen */
	0,           /* traced_port */
	0 /* end */
    };

/* default options for daemon */
osi_options_t osi_options_default_daemon =
    {
	OSI_TRUE,    /* detect_cpu */
	OSI_TRUE,    /* detect_cache */
	OSI_TRUE,    /* start_bkg_threads */
	OSI_TRUE,    /* init_syscall */
	OSI_TRUE,    /* ioctl_syscall_persistent_fd */
	OSI_TRUE,    /* trace_init_syscall */
	OSI_TRUE,    /* trace_gen_rgy_registration */
	OSI_TRUE,    /* trace_start_mail_thread */
	128,         /* trace buffer_size */
	OSI_FALSE,   /* traced_listen */
	0,           /* traced_port */
	0 /* end */
    };

/* default options for command line utility */
osi_options_t osi_options_default_util =
    {
	OSI_FALSE,   /* detect_cpu */
	OSI_FALSE,   /* detect_cache */
	OSI_FALSE,   /* start_bkg_threads */
	OSI_TRUE,    /* init_syscall */
	OSI_FALSE,   /* ioctl_syscall_persistent_fd */
	OSI_FALSE,   /* trace_init_syscall */
	OSI_FALSE,   /* trace_gen_rgy_registration */
	OSI_FALSE,   /* trace_start_mail_thread */
	8,           /* trace buffer_size */
	OSI_FALSE,   /* traced_listen */
	0,           /* traced_port */
	0 /* end */
    };

/* default options for a pam .so linked against libosi */
osi_options_t osi_options_default_pam =
    {
	OSI_FALSE,   /* detect_cpu */
	OSI_FALSE,   /* detect_cache */
	OSI_FALSE,   /* start_bkg_threads */
	OSI_TRUE,    /* init_syscall */
	OSI_FALSE,   /* ioctl_syscall_persistent_fd */
	OSI_FALSE,   /* trace_init_syscall */
	OSI_FALSE,   /* trace_gen_rgy_registration */
	OSI_FALSE,   /* trace_start_mail_thread */
	8,           /* trace buffer_size */
	OSI_FALSE,   /* traced_listen */
	0,           /* traced_port */
	0 /* end */
    };

/* default options structure for every program type
 * indexed by osi_ProgramType_t enumeration value */
osi_options_t * osi_options_defaults[osi_ProgramType_Max_Id+1] =
    {
	&osi_options_default_daemon,      /* osi_ProgramType_Library */
	&osi_options_default_daemon,      /* osi_ProgramType_Bosserver */
#if defined(OSI_KERNELSPACE_ENV)          /* osi_ProgramType_CacheManager */
	&osi_options_default_kernel,
#else
	&osi_options_default_ukernel,
#endif
	&osi_options_default_daemon,      /* osi_ProgramType_Fileserver */
	&osi_options_default_daemon,      /* osi_ProgramType_Volserver */
	&osi_options_default_daemon,      /* osi_ProgramType_Salvager */
	&osi_options_default_daemon,      /* osi_ProgramType_Salvageserver */
	&osi_options_default_daemon,      /* osi_ProgramType_SalvageserverWorker */
	&osi_options_default_daemon,      /* osi_ProgramType_Ptserver */
	&osi_options_default_daemon,      /* osi_ProgramType_Vlserver */
	&osi_options_default_daemon,      /* osi_ProgramType_Kaserver */
	&osi_options_default_daemon,      /* osi_ProgramType_Buserver */
	&osi_options_default_util,        /* osi_ProgramType_Utility */
	&osi_options_default_util,        /* osi_ProgramType_EphemeralUtility */
	&osi_options_default_daemon,      /* osi_ProgramType_TraceCollector */
	&osi_options_default_util,        /* osi_ProgramType_TestSuite */
	&osi_options_default_kernel,      /* osi_ProgramType_TraceKernel */
	&osi_options_default_daemon,      /* osi_ProgramType_Backup */
	&osi_options_default_daemon,      /* osi_ProgramType_BuTC */
	&osi_options_default_util         /* osi_ProgramType_Max_Id */
    };


/* implementation details below this line */

typedef union {
    void * opaque;
    osi_options_val_bool_t * v_bool;
    osi_options_val_int8_t * v_int8;
    osi_options_val_uint8_t * v_uint8;
    osi_options_val_int16_t * v_int16;
    osi_options_val_uint16_t * v_uint16;
    osi_options_val_int32_t * v_int32;
    osi_options_val_uint32_t * v_uint32;
    osi_options_val_int64_t * v_int64;
    osi_options_val_uint64_t * v_uint64;
    osi_options_val_ptr_t * v_ptr;
    osi_options_val_string_t * v_string;
} osi_options_param_val_ptr_t;

osi_static osi_inline osi_result
osi_options_val_validate(osi_options_param_t param,
			 osi_options_val_t * val);
osi_static osi_result
osi_options_val_ptr_setup(osi_options_t * opt,
			  osi_options_param_t param,
			  osi_options_param_val_ptr_t * v_ptr);
osi_static osi_result
osi_options_val_set(osi_options_param_val_ptr_t * dst,
		    osi_options_val_t * src);
osi_static osi_result
osi_options_val_get(osi_options_val_t * dst,
		    osi_options_param_val_ptr_t * src);



osi_static osi_inline osi_result
osi_options_val_validate(osi_options_param_t param,
			 osi_options_val_t * val)
{
    osi_result res = OSI_OK;
    if (val->type != osi_option_types[param]) {
	res = OSI_FAIL;
    }
    return res;
}

#define OSI_OPTION_SWITCH_CASE(param) \
    case OSI_OPTION_##param: \
        v_ptr->opaque = &opt->param; \
        break

osi_static osi_result
osi_options_val_ptr_setup(osi_options_t * opt,
			  osi_options_param_t param,
			  osi_options_param_val_ptr_t * v_ptr)
{
    osi_result res = OSI_OK;

    switch (param) {
	OSI_OPTION_SWITCH_CASE(DETECT_CPU);
	OSI_OPTION_SWITCH_CASE(DETECT_CACHE);
	OSI_OPTION_SWITCH_CASE(START_BKG_THREADS);
	OSI_OPTION_SWITCH_CASE(INIT_SYSCALL);
	OSI_OPTION_SWITCH_CASE(IOCTL_SYSCALL_PERSISTENT_FD);
	OSI_OPTION_SWITCH_CASE(TRACE_INIT_SYSCALL);
	OSI_OPTION_SWITCH_CASE(TRACE_GEN_RGY_REGISTRATION);
	OSI_OPTION_SWITCH_CASE(TRACE_START_MAIL_THREAD);
	OSI_OPTION_SWITCH_CASE(TRACE_BUFFER_SIZE);
	OSI_OPTION_SWITCH_CASE(TRACED_LISTEN);
	OSI_OPTION_SWITCH_CASE(TRACED_PORT);
    default:
	res = OSI_FAIL;
    }

    return res;
}

osi_static osi_result
osi_options_val_set(osi_options_param_val_ptr_t * dst,
		     osi_options_val_t * src)
{
    osi_result res = OSI_OK;

    switch (src->type) {
    case OSI_OPTION_VAL_TYPE_BOOL:
	*(dst->v_bool) = src->val.v_bool;
	break;
    case OSI_OPTION_VAL_TYPE_INT8:
	*(dst->v_int8) = src->val.v_int8;
	break;
    case OSI_OPTION_VAL_TYPE_UINT8:
	*(dst->v_uint8) = src->val.v_uint8;
	break;
    case OSI_OPTION_VAL_TYPE_INT16:
	*(dst->v_int16) = src->val.v_int16;
	break;
    case OSI_OPTION_VAL_TYPE_UINT16:
	*(dst->v_uint16) = src->val.v_uint16;
	break;
    case OSI_OPTION_VAL_TYPE_INT32:
	*(dst->v_int32) = src->val.v_int32;
	break;
    case OSI_OPTION_VAL_TYPE_UINT32:
	*(dst->v_uint32) = src->val.v_uint32;
	break;
    case OSI_OPTION_VAL_TYPE_INT64:
	*(dst->v_int64) = src->val.v_int64;
	break;
    case OSI_OPTION_VAL_TYPE_UINT64:
	*(dst->v_uint64) = src->val.v_uint64;
	break;
    case OSI_OPTION_VAL_TYPE_PTR:
	*(dst->v_ptr) = src->val.v_ptr;
	break;
    case OSI_OPTION_VAL_TYPE_STRING:
	*(dst->v_string) = src->val.v_string;
	break;
    default:
	res = OSI_FAIL;
    }

    return res;
}

osi_static osi_result
osi_options_val_get(osi_options_val_t * dst,
		    osi_options_param_val_ptr_t * src)
{
    osi_result res = OSI_OK;

    switch (dst->type) {
    case OSI_OPTION_VAL_TYPE_BOOL:
	dst->val.v_bool = *(src->v_bool);
	break;
    case OSI_OPTION_VAL_TYPE_INT8:
	dst->val.v_int8 = *(src->v_int8);
	break;
    case OSI_OPTION_VAL_TYPE_UINT8:
	dst->val.v_uint8 = *(src->v_uint8);
	break;
    case OSI_OPTION_VAL_TYPE_INT16:
	dst->val.v_int16 = *(src->v_int16);
	break;
    case OSI_OPTION_VAL_TYPE_UINT16:
	dst->val.v_uint16 = *(src->v_uint16);
	break;
    case OSI_OPTION_VAL_TYPE_INT32:
	dst->val.v_int32 = *(src->v_int32);
	break;
    case OSI_OPTION_VAL_TYPE_UINT32:
	dst->val.v_uint32 = *(src->v_uint32);
	break;
    case OSI_OPTION_VAL_TYPE_INT64:
	dst->val.v_int64 = *(src->v_int64);
	break;
    case OSI_OPTION_VAL_TYPE_UINT64:
	dst->val.v_uint64 = *(src->v_uint64);
	break;
    case OSI_OPTION_VAL_TYPE_PTR:
	dst->val.v_ptr = *(src->v_ptr);
	break;
    case OSI_OPTION_VAL_TYPE_STRING:
	dst->val.v_string = *(src->v_string);
	break;
    default:
	res = OSI_FAIL;
    }

    return res;
}

osi_result
osi_options_Init(osi_ProgramType_t programType, 
		 osi_options_t * opts)
{
    osi_result res = OSI_OK;
    osi_options_t * defaults;

    if (osi_compiler_expect_false(programType >= osi_ProgramType_Max_Id)) {
	res = OSI_FAIL;
	goto error;
    }

    defaults = osi_options_defaults[programType];
    if (osi_compiler_expect_false(defaults == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    res = osi_options_Copy(opts, defaults);

 error:
    return res;
}

osi_result
osi_options_Destroy(osi_options_t * opts)
{
    return OSI_OK;
}

osi_result
osi_options_Copy(osi_options_t * dst, osi_options_t * src)
{
    osi_mem_copy(dst, src, sizeof(osi_options_t));
    return OSI_OK;
}

osi_result
osi_options_Set(osi_options_t * opt,
		osi_options_param_t param,
		osi_options_val_t * val)
{
    osi_result res;
    osi_options_param_val_ptr_t v_ptr;

    /* verify param and setup v_ptr */
    res = osi_options_val_ptr_setup(opt, param, &v_ptr);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    /* make sure passed in value is of appropriate type */
    res = osi_options_val_validate(param, val);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    /* do option value assignment */
    res = osi_options_val_set(&v_ptr, val);

 error:
    return res;
}

osi_result
osi_options_Get(osi_options_t * opt,
		osi_options_param_t param,
		osi_options_val_t * val)
{
    osi_result res = OSI_OK;
    osi_options_param_val_ptr_t v_ptr;

    res = osi_options_val_ptr_setup(opt, param, &v_ptr);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    /* do option value assignment */
    val->type = osi_option_types[param];
    res = osi_options_val_get(val, &v_ptr);

 error:
    return res;
}
