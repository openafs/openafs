/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_object_init.h>
#include <osi/osi_mutex.h>

#if defined(OSI_ENV_KERNELSPACE)
osi_mutex_t __osi_object_init_lock;

OSI_INIT_FUNC_DECL(osi_object_init_PkgInit)
{
    osi_result res = OSI_OK;

    osi_mutex_Init(&__osi_object_init_lock,
		   osi_impl_mutex_opts());

 error:
    return res;
}

OSI_FINI_FUNC_DECL(osi_object_init_PkgShutdown)
{
    osi_result res = OSI_OK;

    osi_mutex_Destroy(&__osi_object_init_lock);

 error:
    return res;
}
#endif /* OSI_ENV_KERNELSPACE */

