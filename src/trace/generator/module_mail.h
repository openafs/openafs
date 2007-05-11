/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_MODULE_MAIL_H
#define _OSI_TRACE_GENERATOR_MODULE_MAIL_H 1


/*
 * osi tracing framework
 * trace module registry
 * mail interface
 */

#if defined(OSI_ENV_USERSPACE)
osi_extern osi_result osi_trace_module_msg_PkgInit(void);
osi_extern osi_result osi_trace_module_msg_PkgShutdown(void);
#endif


#endif /* _OSI_TRACE_GENERATOR_MODULE_MAIL_H */
