/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_SPINLOCK_OPTIONS_H
#define _OSI_COMMON_SPINLOCK_OPTIONS_H 1

#include <osi/osi_mem.h>

typedef struct osi_spinlock_options {
    osi_uint8 preemptive_only;     /* only activate in pre-emptive environments (e.g. no-op for LWP) */
    osi_uint8 trace_allowed;       /* whether or not lock tracing is allowed */
    osi_uint8 trace_enabled;       /* enable lock tracing */
} osi_spinlock_options_t;
/* defaults:  { 0, 1, 0 } */

typedef enum {
    OSI_SPINLOCK_OPTION_PREEMPTIVE_ONLY,
    OSI_SPINLOCK_OPTION_TRACE_ALLOWED,
    OSI_SPINLOCK_OPTION_TRACE_ENABLED,
    OSI_SPINLOCK_OPTION_MAX_ID
} osi_spinlock_options_param_t;

#define osi_spinlock_options_Init(opt) \
    osi_Macro_Begin \
        (opt)->preemptive_only = 0; \
        (opt)->trace_allowed = 1; \
        (opt)->trace_enabled = 0; \
    osi_Macro_End

#define osi_spinlock_options_Destroy(opt)

#define osi_spinlock_options_Copy(dst, src) \
    osi_Macro_Begin \
        if (src) { \
            osi_mem_copy((dst), (src), sizeof(osi_spinlock_options_t)); \
        } else { \
            osi_spinlock_options_Init(dst); \
        } \
    osi_Macro_End

osi_extern void osi_spinlock_options_Set(osi_spinlock_options_t * opt,
					 osi_spinlock_options_param_t param,
					 long val);

#endif /* _OSI_COMMON_SPINLOCK_OPTIONS_H */
