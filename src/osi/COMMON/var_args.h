/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_VAR_ARGS_H
#define _OSI_COMMON_VAR_ARGS_H 1


/*
 * back-end implementation notes:
 *
 * default implementations of all
 * major interfaces exist in src/osi/COMMON
 * their compilation may be disabled by
 * defining the following macro in the 
 * platform header:
 *
 *  OSI_VAR_ARGS_OVERRIDE
 *    -- disables compilation of generic osi_var_args interfaces
 *
 *  OSI_VAR_ARGS_OVERRIDE_INCLUDE
 *    -- disables stdarg.h include
 *
 */


#if !defined(OSI_VAR_ARGS_OVERRIDE)

#if !defined(OSI_VAR_ARGS_OVERRIDE_INCLUDE)
#include <stdarg.h>
#endif

typedef va_list osi_va_list;

#define osi_va_start(list, arg) va_start(list, arg)
#define osi_va_copy(dest, src) va_copy(dest, src)
#define osi_va_arg(list, type) va_arg(list, type)
#define osi_va_end(list) va_end(list)

#endif /* !OSI_VAR_ARGS_OVERRIDE */

#endif /* _OSI_COMMON_VAR_ARGS_H */
