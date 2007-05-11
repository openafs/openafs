/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_THREAD_BIND_H
#define _OSI_COMMON_THREAD_BIND_H 1

/*
 * osi abstraction
 * thread api
 * bound thread support
 */

#include <osi/osi_cpu.h>

#if defined(OSI_IMPLEMENTS_CPU_BIND)
#define OSI_IMPLEMENTS_THREAD_BIND 1
#define osi_thread_bind_current(cpu)  osi_cpu_bind_thread_current(cpu)
#define osi_thread_unbind_current()   osi_cpu_unbind_thread_current()
#else /* !OSI_IMPLEMENTS_CPU_BIND */
#define osi_thread_bind_current(cpu)  (OSI_ERROR_NOT_SUPPORTED)
#define osi_thread_unbind_current()   (OSI_ERROR_NOT_SUPPORTED)
#endif /* !OSI_IMPLEMENTS_CPU_BIND */


#endif /* _OSI_COMMON_THREAD_BIND_H */
