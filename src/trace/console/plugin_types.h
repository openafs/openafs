/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSOLE_PLUGIN_TYPES_H
#define _OSI_TRACE_CONSOLE_PLUGIN_TYPES_H 1

/*
 * osi tracing framework
 * trace console
 * types for plugin support
 */

typedef osi_result osi_trace_console_plugin_init_func_t(void * sdata);
typedef osi_result osi_trace_console_plugin_fini_func_t(void * sdata);

typedef struct {
    osi_trace_console_plugin_init_func_t * initializer;
    osi_trace_console_plugin_fini_func_t * finalizer;
    void * sdata;
    int error_is_fatal;
    osi_list_element plugin_list;
} osi_trace_console_plugin_registration_t;

#endif /* _OSI_TRACE_CONSOLE_PLUGIN_TYPES_H */
