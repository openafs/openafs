/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_DATAMODEL_H
#define _OSI_OSI_DATAMODEL_H 1


/*
 * platform-independent osi_datamodel API
 *
 *  OSI_DATAMODEL -- for kernelspace -- datamodel of kernel context
 *                   for userspace -- datamodel of this context
 *                   (values are macros OSI_ILP32_ENV, OSI_LP64_ENV, etc.)
 *
 *  OSI_DATAMODEL_IS(x)
 *    -- whether datamodel of current compilation is x
 *
 *  osi_datamodel_t -- context datamodel type
 *
 *  osi_datamodel_t osi_datamodel();
 *    -- get the datamodel
 *
 *  unsigned int osi_datamodel_int();
 *    -- get the datamodel int size
 *
 *  unsigned int osi_datamodel_long();
 *    -- get the datamodel long size
 *
 *  unsigned int osi_datamodel_pointer();
 *    -- get the datamodel pointer size
 *
 */


#define OSI_DATAMODEL_IS(x)  (OSI_DATAMODEL == x)


#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/datamodel.h>
#elif defined(OSI_HPUX_ENV)
#include <osi/HPUX/datamodel.h>
#elif defined(OSI_AIX_ENV)
#include <osi/AIX/datamodel.h>
#elif defined(OSI_LINUX20_ENV)
#include <osi/LINUX/datamodel.h>
#else
#error "please port the osi_datamodel api to your platform"
#endif

#else /* !OSI_ENV_KERNELSPACE */

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/datamodel.h>
#elif defined(OSI_HPUX_ENV)
#include <osi/HPUX/datamodel.h>
#elif defined(OSI_AIX_ENV)
#include <osi/AIX/datamodel.h>
#elif defined(OSI_LINUX20_ENV)
#include <osi/LINUX/datamodel.h>
#else
#error "please port the osi_datamodel api to your platform"
#endif

#endif /* !OSI_ENV_KERNELSPACE */

#endif /* _OSI_OSI_DATAMODEL_H */
