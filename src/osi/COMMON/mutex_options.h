/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_MUTEX_OPTIONS_H
#define _OSI_COMMON_MUTEX_OPTIONS_H 1

#include <osi/osi_mem.h>

#define osi_mutex_options_Init(opt) \
    osi_Macro_Begin \
        (opt)->preemptive_only = 0; \
        (opt)->trace_allowed = 1; \
        (opt)->trace_enabled = 0; \
    osi_Macro_End

#define osi_mutex_options_Destroy(opt)

#define osi_mutex_options_Copy(dst, src) \
    osi_Macro_Begin \
        if (src) { \
            osi_mem_copy((dst), (src), sizeof(osi_mutex_options_t)); \
        } else { \
            osi_mutex_options_Init(dst); \
        } \
    osi_Macro_End

osi_extern void osi_mutex_options_Set(osi_mutex_options_t * opt,
				      osi_mutex_options_param_t param,
				      long val);

#endif /* _OSI_COMMON_MUTEX_OPTIONS_H */
