/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_KTHREAD_H
#define	_OSI_LEGACY_KTHREAD_H


typedef int osi_thread_id_t;
typedef int * osi_thread_p;

#define osi_thread_PkgInit() osi_thread_event_PkgInit()
#define osi_thread_PkgShutdown() osi_thread_event_PkgShutdown()

#define osi_thread_create(stk, stk_size, proc, args, args_len, pp, pri)
#define osi_thread_current_id() 0
#define osi_thread_id(t) (*(t))

#define osi_thread_current() osi_NULL
#define osi_thread_to_proc(x) osi_NULL


#endif /* _OSI_LEGACY_KTHREAD_H */
