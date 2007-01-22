/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KMEM_LOCAL_H
#define	_OSI_SOLARIS_KMEM_LOCAL_H

/*
 * cpu-local memory support
 */


/*
 * Solaris does not support kernel module cpu-specific data
 * so we need to turn on the compat flag
 */
#define OSI_MEM_LOCAL_CPU_ARRAY 1


#include <osi/osi_kernel.h>
#include <osi/COMMON/mem_local.h>


#define osi_mem_local_os_PkgInit()       (OSI_OK)
#define osi_mem_local_os_PkgShutdown()   (OSI_OK)


#endif /* _OSI_SOLARIS_KMEM_LOCAL_H */
