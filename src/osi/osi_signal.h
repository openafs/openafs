/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_SIGNAL_H
#define _OSI_OSI_SIGNAL_H 1


/*
 * platform-independent osi_event API
 *
 *  osi_signal_t
 *    -- signal id
 *
 *  osi_result osi_signal_raise(osi_proc_p, osi_signal_t);
 *    -- send a signal to a process
 *
 * the following API elements require:
 * defined(OSI_USERSPACE_ENV)
 *
 *  osi_signal_handler_t
 *    -- signal handler function prototype
 *
 *  osi_signal_action_t
 *    -- sigaction type
 *
 *  osi_signal_info_t
 *    -- siginfo type
 *
 *  osi_signal_action_func_t
 *    -- sigaction handler function prototype
 *
 *  OSI_SIGNAL_DFL
 *    -- default signal handler
 *
 *  OSI_SIGNAL_IGN
 *    -- no-op signal handler
 *
 *  osi_signal_handler_t * osi_signal_handler(osi_signal_t, 
 *                                            osi_signal_handler_t *);
 *    -- set signal handler, and return old handler
 *
 *
 */

#if defined(OSI_KERNELSPACE_ENV)

/* XXX write me */

#else /* !OSI_KERNELSPACE_ENV */

#include <osi/COMMON/usignal.h>

#endif /* !OSI_KERNELSPACE_ENV */

#endif /* _OSI_OSI_SIGNAL_H */
