/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_OPTIONS_H
#define _OSI_OSI_OPTIONS_H 1


/*
 * libosi
 * global initialization options
 *
 * types:
 *
 *  osi_options_t
 *    -- libosi options control structure
 *
 *  osi_options_param_t
 *    -- enumeration of all available osi options
 *
 *  osi_options_val_t
 *    -- union of possible value types
 *
 * globals:
 *
 *  osi_options_t osi_options_default_kernel
 *    -- default options for kernel
 *
 * interfaces:
 *
 *  osi_result osi_options_Init(osi_ProgramType_t, osi_options_t *);
 *    -- initialize an options structure
 *
 *  osi_result osi_options_Destroy(osi_options_t *);
 *    -- destroy an options structure
 *
 *  osi_result osi_options_Copy(osi_options_t * dst, osi_options_t * src);
 *    -- initialize $dst$, and copy values from $src$ into $dst$
 *
 *  osi_result osi_options_Set(osi_options_t * opt, 
 *                             osi_options_param_t param, 
 *                             osi_options_val_t * val);
 *    -- set the value of parameter $param$ to value $val$ in options
 *       structure $opt$
 *
 *  osi_result osi_options_Get(osi_options_t * opt,
 *                             osi_options_param_t param,
 *                             osi_options_val_t * val);
 *    -- get the value $val$ of parameter $param$ from options structure $opt$
 */


/*
 * if you edit anything below this line, please make sure
 * you keep src/osi/COMMON/libosi_options.c in sync!!!
 */

typedef osi_bool_t osi_options_val_bool_t;
typedef osi_int8 osi_options_val_int8_t;
typedef osi_uint8 osi_options_val_uint8_t;
typedef osi_int16 osi_options_val_int16_t;
typedef osi_uint16 osi_options_val_uint16_t;
typedef osi_int32 osi_options_val_int32_t;
typedef osi_uint32 osi_options_val_uint32_t;
typedef osi_int64 osi_options_val_int64_t;
typedef osi_uint64 osi_options_val_uint64_t;
typedef void * osi_options_val_ptr_t;
typedef char * osi_options_val_string_t;

typedef enum {
    OSI_OPTION_VAL_TYPE_INVALID,
    OSI_OPTION_VAL_TYPE_BOOL,
    OSI_OPTION_VAL_TYPE_INT8,
    OSI_OPTION_VAL_TYPE_UINT8,
    OSI_OPTION_VAL_TYPE_INT16,
    OSI_OPTION_VAL_TYPE_UINT16,
    OSI_OPTION_VAL_TYPE_INT32,
    OSI_OPTION_VAL_TYPE_UINT32,
    OSI_OPTION_VAL_TYPE_INT64,
    OSI_OPTION_VAL_TYPE_UINT64,
    OSI_OPTION_VAL_TYPE_PTR,
    OSI_OPTION_VAL_TYPE_STRING,
    OSI_OPTION_VAL_TYPE_MAX_ID
} osi_options_val_type_t;

typedef union {
    osi_options_val_bool_t v_bool;
    osi_options_val_int8_t v_int8;
    osi_options_val_uint8_t v_uint8;
    osi_options_val_int16_t v_int16;
    osi_options_val_uint16_t v_uint16;
    osi_options_val_int32_t v_int32;
    osi_options_val_uint32_t v_uint32;
    osi_options_val_int64_t v_int64;
    osi_options_val_uint64_t v_uint64;
    osi_options_val_ptr_t v_ptr;
    osi_options_val_string_t v_string;
} osi_options_val_u_t;

typedef struct {
    osi_options_val_type_t type;
    osi_options_val_u_t val;
} osi_options_val_t;

typedef struct osi_options {
    osi_options_val_bool_t DETECT_CPU;
    osi_options_val_bool_t DETECT_CACHE;
    osi_options_val_bool_t START_BKG_THREADS;
    osi_options_val_bool_t INIT_SYSCALL;
    osi_options_val_bool_t IOCTL_SYSCALL_PERSISTENT_FD;
    osi_options_val_bool_t TRACE_INIT_SYSCALL;
    osi_options_val_bool_t TRACE_GEN_RGY_REGISTRATION;
    osi_options_val_bool_t TRACE_START_MAIL_THREAD;
    osi_options_val_uint32_t TRACE_BUFFER_SIZE;
    osi_options_val_bool_t TRACED_LISTEN;
    osi_options_val_uint16_t TRACED_PORT;
    int terminal;
} osi_options_t;

typedef enum {
    OSI_OPTION_MIN_ID,
    OSI_OPTION_DETECT_CPU,
    OSI_OPTION_DETECT_CACHE,
    OSI_OPTION_START_BKG_THREADS,
    OSI_OPTION_INIT_SYSCALL,
    OSI_OPTION_IOCTL_SYSCALL_PERSISTENT_FD,
    OSI_OPTION_TRACE_INIT_SYSCALL,
    OSI_OPTION_TRACE_GEN_RGY_REGISTRATION,
    OSI_OPTION_TRACE_START_MAIL_THREAD,
    OSI_OPTION_TRACE_BUFFER_SIZE,
    OSI_OPTION_TRACED_LISTEN,
    OSI_OPTION_TRACED_PORT,
    OSI_OPTION_MAX_ID
} osi_options_param_t;


#include <osi/COMMON/libosi_options.h>

#endif /* _OSI_OSI_OPTIONS_H */
