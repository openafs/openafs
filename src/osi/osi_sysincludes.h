/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_SYSINCLUDES_H
#define _OSI_OSI_SYSINCLUDES_H 1


/*
 * osi abstraction
 * system includes needed by the osi abstraction
 */

#if defined(OSI_ENV_KERNELSPACE)
#include <osi/COMMON/ksysincludes.h>
#else /* !OSI_ENV_KERNELSPACE */
#include <osi/COMMON/usysincludes.h>
#endif /* !OSI_ENV_KERNELSPACE */

#endif /* _OSI_OSI_SYSINCLUDES_H */
