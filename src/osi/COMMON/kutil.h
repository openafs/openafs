/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
 */

#ifndef _OSI_COMMON_KUTIL_H
#define _OSI_COMMON_KUTIL_H 1

#include <osi/osi_kernel.h>

/* pulled from rx_kernel.h */
osi_extern int osi_utoa(char *buf, size_t len, unsigned long val);

#if !defined(OSI_UTIL_OVERRIDE_MSG)
#define	osi_Msg printf)(
#endif

#if !defined(OSI_UTIL_OVERRIDE_MSG_CONSOLE)
#define osi_Msg_console osi_kernel_printf
#endif /* !OSI_UTIL_OVERRIDE_MSG_CONSOLE */

#if !defined(OSI_UTIL_OVERRIDE_MSG_USER)
#define osi_Msg_user osi_kernel_uprintf
#endif /* !OSI_UTIL_OVERRIDE_MSG_CONSOLE */

#endif /* _OSI_COMMON_KUTIL_H */
