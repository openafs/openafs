/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_THREAD_OPTION_H
#define _OSI_COMMON_THREAD_OPTION_H 1

#include <osi/osi_mem.h>

typedef struct osi_thread_options {
    osi_uint8 detached;            /* create thread with detached option enabled */
    osi_uint8 trace_allowed;       /* whether or not thread tracing is allowed */
} osi_thread_options_t;
/* defaults:  { 0, 1 } */

typedef enum {
    OSI_THREAD_OPTION_DETACHED,
    OSI_THREAD_OPTION_TRACE_ALLOWED,
    OSI_THREAD_OPTION_MAX_ID
} osi_thread_options_param_t;

#define osi_thread_options_Init(opt) \
    osi_Macro_Begin \
        (opt)->detached = 0; \
        (opt)->trace_allowed = 1; \
    osi_Macro_End

#define osi_thread_options_Destroy(opt)

#define osi_thread_options_Copy(dst, src) \
    osi_Macro_Begin \
        if (src) { \
            osi_mem_copy((dst), (src), sizeof(osi_thread_options_t)); \
        } else { \
            osi_thread_options_Init(dst); \
        } \
    osi_Macro_End

osi_extern void osi_thread_options_Set(osi_thread_options_t * opt,
				       osi_thread_options_param_t param,
				       long val);

#endif /* _OSI_COMMON_THREAD_OPTION_H */
