/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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
 * this entire API requires:
 * defined(OSI_IMPLEMENTS_SIGNAL)
 *
 *
 * macros:
 *
 *  OSI_SIGNAL_COUNT
 *    -- total number of signals supported on this platform
 *
 * types:
 *
 *  osi_signal_t
 *    -- signal id
 *
 *  osi_result osi_signal_raise(osi_proc_p, osi_signal_t);
 *    -- send a signal to a process
 *
 *
 * the following API elements require:
 * defined(OSI_ENV_USERSPACE)
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
 * signal set API:
 *
 * types:
 *
 *  osi_signal_set_t
 *    -- sigset type
 *
 * interfaces:
 *
 *  void
 *  osi_signal_set_empty(osi_signal_set_t *);
 *    -- initialize to the empty set
 *
 *  void
 *  osi_signal_set_full(osi_signal_set_t *);
 *    -- initialize to the full set
 *
 *  void
 *  osi_signal_set_add(osi_signal_set_t *,
 *                     osi_signal_t);
 *    -- add a signal to the sigset
 *
 *  void
 *  osi_signal_set_del(osi_signal_set_t *,
 *                     osi_signal_t);
 *    -- delete a signal from the sigset
 *
 *  osi_bool_t
 *  osi_signal_set_isMember(osi_signal_set_t *,
 *                          osi_signal_t);
 *    -- return TRUE if signal is a member of the sigset
 *
 *
 * thread sigmask API
 *
 * types:
 *
 *  OSI_SIGNAL_BLOCK
 *    -- block all signals in set
 *
 *  OSI_SIGNAL_UNBLOCK
 *    -- unblock all signals in set
 *
 *  OSI_SIGNAL_SETMASK
 *    -- set signal mask to passed set
 *
 * interface:
 *
 *  osi_result
 *  osi_signal_mask_set(op,
 *                      osi_signal_set_t * new,
 *                      osi_signal_set_t * old);
 *    -- perform operation $op$ (one of OSI_SIGNAL_BLOCK, 
 *       OSI_SIGNAL_UNBLOCK, or OSI_SIGNAL_SETMASK).
 *       Signal set $new$ will be used to update the thread's
 *       sigmask.  The old value of the mask is passed back
 *       into $old$, if $old$ is not NULL
 *
 *  osi_result
 *  osi_signal_mask_get(osi_signal_set_t *);
 *    -- get the current signal mask
 *
 * threads softsig environment API requires:
 * defined(OSI_IMPLEMENTS_SIGNAL_SOFTSIG)
 *
 *  osi_result
 *  osi_signal_soft_register(osi_signal_t,
 *                           osi_signal_handler_t *);
 *    -- register a softsig handler for a signal
 *
 *  osi_result
 *  osi_signal_soft_unregister(osi_signal_t,
 *                             osi_signal_handler_t *);
 *    -- unregister a softsig handler for a signal
 */

#if defined(OSI_ENV_KERNELSPACE)

/* XXX write me */

#else /* !OSI_ENV_KERNELSPACE */

#if defined(OSI_ENV_UNIX)
#include <osi/UNIX/usignal.h>
#endif /* OSI_ENV_UNIX */

#endif /* !OSI_ENV_KERNELSPACE */

#endif /* _OSI_OSI_SIGNAL_H */
