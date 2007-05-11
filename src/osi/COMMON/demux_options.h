/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_DEMUX_OPTIONS_H
#define _OSI_COMMON_DEMUX_OPTIONS_H 1

#include <osi/osi_mem.h>

/* demux object options */

typedef struct osi_demux_options {
    osi_bool_t trace_allowed;       /* whether or not demux tracing is allowed */
    osi_bool_t trace_enabled;       /* enable demux tracing */
} osi_demux_options_t;
/* defaults:  { 1, 0 } */

typedef enum {
    OSI_DEMUX_OPTION_TRACE_ALLOWED,
    OSI_DEMUX_OPTION_TRACE_ENABLED,
    OSI_DEMUX_OPTION_MAX_ID
} osi_demux_options_param_t;

#define osi_demux_options_Init(opt) \
    osi_Macro_Begin \
        (opt)->trace_allowed = 1; \
        (opt)->trace_enabled = 0; \
    osi_Macro_End

#define osi_demux_options_Destroy(opt)

#define osi_demux_options_Copy(dst, src) \
    osi_Macro_Begin \
        if (src) { \
            osi_mem_copy((dst), (src), sizeof(osi_demux_options_t)); \
        } else { \
            osi_demux_options_Init(dst); \
        } \
    osi_Macro_End

osi_extern osi_result osi_demux_options_Set(osi_demux_options_t * opt,
					    osi_demux_options_param_t param,
					    osi_options_val_t * val);


/* demux action object options */

typedef struct {
    osi_bool_t error_is_fatal;
    osi_bool_t terminal;
} osi_demux_action_options_t;
/* defaults: { 0, 0 } */

typedef enum {
    OSI_DEMUX_ACTION_OPTION_ERROR_IS_FATAL,
    OSI_DEMUX_ACTION_OPTION_TERMINAL,
    OSI_DEMUX_ACTION_OPTION_MAX_ID
} osi_demux_action_options_param_t;

#define osi_demux_action_options_Init(opt) \
    osi_Macro_Begin \
        (opt)->error_is_fatal = 0; \
        (opt)->terminal = 0; \
    osi_Macro_End

#define osi_demux_action_options_Destroy(opt)

#define osi_demux_action_options_Copy(dst, src) \
    osi_Macro_Begin \
        if (src) { \
            osi_mem_copy((dst), (src), sizeof(osi_demux_action_options_t)); \
        } else { \
            osi_demux_action_options_Init(dst); \
        } \
    osi_Macro_End

osi_extern osi_result osi_demux_action_options_Set(osi_demux_action_options_t * opt,
						   osi_demux_action_options_param_t param,
						   osi_options_val_t * val);

#endif /* _OSI_COMMON_DEMUX_OPTIONS_H */
