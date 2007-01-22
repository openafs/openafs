/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_TRACED_PLUGIN_H
#define _OSI_TRACE_TRACED_PLUGIN_H 1

/*
 * osi tracing framework
 * trace server
 * plugin support
 */

#include <trace/traced/plugin_types.h>

#define OSI_TRACED_PLUGIN_DECL(node, init_fp, fini_fp, sdata, error_is_fatal) \
osi_traced_plugin_registration_t node = { \
    init_fp, \
    fini_fp, \
    sdata, \
    error_is_fatal \
}

osi_extern osi_result
osi_traced_plugin_register(osi_traced_plugin_registration_t *);

osi_extern osi_result
osi_traced_plugin_option_Set(osi_options_param_t,
			     osi_options_val_t *);

osi_extern osi_result osi_traced_plugin_PkgInit(void);
osi_extern osi_result osi_traced_plugin_PkgShutdown(void);

#endif /* _OSI_TRACE_TRACED_PLUGIN_H */
