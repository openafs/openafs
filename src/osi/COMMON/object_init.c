/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_object_init.h>
#include <osi/osi_mutex.h>

#if defined(OSI_KERNELSPACE_ENV)
osi_mutex_t __osi_object_init_lock;

osi_result 
osi_object_init_PkgInit(void)
{
    osi_result res = OSI_OK;

    osi_mutex_Init(&__osi_object_init_lock,
		   &osi_common_options.mutex_opts);

 error:
    return res;
}

osi_result 
osi_object_init_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    osi_mutex_Destroy(&__osi_object_init_lock);

 error:
    return res;
}
#endif /* OSI_KERNELSPACE_ENV */

