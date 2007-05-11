/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_UTIL_H
#define _OSI_OSI_UTIL_H 1


/*
 * platform-independent osi_util API
 *
 *  void osi_Panic(msg, ...)
 *    -- panic the kernel or current process context with a
 *       printf-style message and up to 3 parameters
 *
 * assertion API:
 *
 *  osi_Assert(expression)
 *    -- panic the kernel or current process context with an
 *       assertion failure message if $expression$ evaluates
 *       to zero
 *
 *  osi_AssertOK(expression)
 *    -- panic the kernel or current process context with an
 *       assertion failure message if $expression$ evaluates
 *       to OSI_RESULT_FAIL()
 *
 *  osi_AssertFAIL(expression)
 *    -- panic the kernel or current process context with an
 *       assertion failure message if $expression$ evaluates
 *       to OSI_RESULT_OK()
 *
 *  osi_AssertDebug(expression)
 *    -- panic the kernel or current process context with an
 *       assertion failure message if $expression$ evaluates
 *       to zero.  The assertion code is only compiled if
 *       OSI_DEBUG_ASSERT is turned on.  Otherwise, the
 *       expression is compiled casted to void.
 *  
 *  osi_AssertDebugOK(expression)
 *    -- panic the kernel or current process context with an
 *       assertion failure message if $expression$ evaluates
 *       to OSI_RESULT_FAIL().  The assertion code is only
 *       compiled if OSI_DEBUG_ASSERT is turned on.  Otherwise,
 *       the expression is compiled casted to void.
 *  
 *  osi_AssertDebugFAIL(expression)
 *    -- panic the kernel or current process context with an
 *       assertion failure message if $expression$ evaluates
 *       to OSI_RESULT_OK().  The assertion code is only
 *       compiled if OSI_DEBUG_ASSERT is turned on.  Otherwise,
 *       the expression is compiled casted to void.
 *  
 * logging api:
 *
 *  (osi_Msg)(msg, ...)
 *    -- emit a printf-style message
 *
 *  void osi_Msg_console(const char * msg, ...);
 *    -- emit a printf-style message to the system console
 *
 *  void osi_Msg_user(const char * msg, ...);
 *    -- emit a printf-style message to the user
 *       (via uprintf(...) in kernelspace;
 *        fprintf(stderr,...) in userspace)
 *
 * program type api:
 *
 *  const char * osi_ProgramTypeToString(osi_ProgramType_t);
 *    -- return string for this program type
 *
 *  const char * osi_ProgramTypeToShortName(osi_ProgramType_t);
 *    -- return an abbreviated string for this program type
 *
 *  osi_result osi_ProgramTypeFromShortName(const char *, osi_ProgramType_t *);
 *    -- return a program type for this abbreviated name
 *
 * the following interfaces are only available if:
 * defined(OSI_ENV_USERSPACE)
 *
 *  void osi_Abort(void);
 *    -- abort the userspace process context
 *
 *  void osi_Exit(int code);
 *    -- exit the process context with return code $code$
 */

/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_LINUX20_ENV)
#include <osi/LINUX/kutil.h>
#elif defined(OSI_AIX_ENV)
#include <osi/AIX/kutil.h>
#endif

#include <osi/COMMON/kutil.h>

#else /* !OSI_ENV_KERNELSPACE */

#include <osi/COMMON/uutil.h>

#endif

#include <osi/COMMON/util.h>

#endif /* _OSI_OSI_UTIL_H */
