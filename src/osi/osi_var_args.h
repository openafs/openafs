/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_VAR_ARGS_H
#define _OSI_OSI_VAR_ARGS_H 1


/*
 * platform-independent osi_var_args API
 *
 *  osi_va_list args;
 *
 *  osi_va_start(osi_va_list args, last named arg);
 *    -- start varargs processing
 *
 *  osi_va_copy(osi_va_list dest, osi_va_list src);
 *    -- copy varargs data
 *
 *  osi_va_end(osi_va_list args);
 *    -- end varargs processing
 *
 *  type osi_va_arg(osi_va_list args, type);
 *    -- return the next argument
 *
 */

#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kvar_args.h>
#endif

#else /* !OSI_ENV_KERNELSPACE */

#endif /* !OSI_ENV_KERNELSPACE */


#include <osi/COMMON/var_args.h>

#endif /* _OSI_OSI_VAR_ARGS_H */
