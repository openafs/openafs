/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSOLE_TRAP_H
#define _OSI_TRACE_CONSOLE_TRAP_H 1

/*
 * osi tracing framework
 * trace console
 * types for trap support
 */

#include <trace/console/trap_types.h>

osi_extern osi_result
osi_trace_console_trap_handler_register(osi_trace_console_trap_handler_t *,
					void * sdata);
osi_extern osi_result
osi_trace_console_trap_handler_unregister(osi_trace_console_trap_handler_t *);

osi_extern osi_result
osi_trace_console_trap_handler_PkgInit(void);
osi_extern osi_result
osi_trace_console_trap_handler_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSOLE_TRAP_H */
