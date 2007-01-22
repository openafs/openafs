/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_USIGNAL_H
#define _OSI_COMMON_USIGNAL_H 1

/*
 * osi process userspace signal api
 */

#include <signal.h>
#include <osi/osi_proc.h>

typedef int osi_signal_t;
typedef struct sigaction osi_signal_action_t;
typedef siginfo_t osi_signal_info_t;

typedef void osi_signal_handler_t(osi_signal_t);
typedef void osi_signal_action_func_t(osi_signal_t, 
				      osi_signal_info_t *,
				      void *);

#define OSI_SIGNAL_DFL  SIG_DFL
#define OSI_SIGNAL_IGN  SIG_IGN

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/COMMON/usignal_inline.h>
#endif

#endif /* _OSI_COMMON_USIGNAL_H */
