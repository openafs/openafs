/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_AGENT_TRAP_H
#define _OSI_TRACE_AGENT_TRAP_H 1

/*
 * osi tracing framework
 * trace consumer process
 * remote trap support
 */

#include <trace/encoding/trap_types.h>



osi_extern osi_result osi_trace_trap_PkgInit(void);
osi_extern osi_result osi_trace_trap_PkgShutdown(void);

#endif /* _OSI_TRACE_AGENT_TRAP_H */
