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

#ifndef _OSI_COMMON_UUTIL_H
#define _OSI_COMMON_UUTIL_H 1

#include <stdio.h>

/* pulled from rx_user.h */
#if !defined(OSI_UTIL_OVERRIDE_MSG)
#define	osi_Msg			    fprintf)(stderr,
#endif

#if !defined(OSI_UTIL_OVERRIDE_ABORT)
#define osi_Abort() abort()
#endif

#if !defined(OSI_UTIL_OVERRIDE_EXIT)
#define osi_Exit(code) exit(code)
#endif

#if !defined(OSI_UTIL_OVERRIDE_MSG_CONSOLE)
osi_extern void osi_Msg_console(const char * msg, ...);
#endif /* !OSI_UTIL_OVERRIDE_MSG_CONSOLE */

#if !defined(OSI_UTIL_OVERRIDE_MSG_USER)
osi_extern void osi_Msg_user(const char * msg, ...);
#endif /* !OSI_UTIL_OVERRIDE_MSG_CONSOLE */


#endif /* _OSI_COMMON_UUTIL_H */
