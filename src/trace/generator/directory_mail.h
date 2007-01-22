/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_DIRECTORY_MAIL_H
#define _OSI_TRACE_GENERATOR_DIRECTORY_MAIL_H 1


/*
 * osi tracing framework
 * probe directory system
 * mail interface
 */

#if defined(OSI_USERSPACE_ENV)
osi_extern osi_result osi_trace_directory_msg_PkgInit(void);
osi_extern osi_result osi_trace_directory_msg_PkgShutdown(void);
#endif


#endif /* _OSI_TRACE_GENERATOR_DIRECTORY_MAIL_H */
