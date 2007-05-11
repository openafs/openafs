/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_PTHREAD_UMEM_LOCAL_H
#define _OSI_PTHREAD_UMEM_LOCAL_H 1


/*
 * thread-local memory support
 */

#include <pthread.h>
#include <assert.h>

/*
 * bring in the common support code
 */
#include <osi/COMMON/mem_local.h>


/* 
 * in order to reduce namespace pollution and keep
 * pthreads from running out of keys, we will add
 * an extra layer of indirection and store all
 * osi mem local keys within our own local osi
 * mem local key namespace
 */
osi_extern pthread_key_t osi_mem_local_pthread_key;

osi_extern osi_result osi_mem_local_os_PkgInit(void);
#define osi_mem_local_os_PkgShutdown()   (OSI_OK)


osi_static osi_inline struct osi_mem_local_ctx_data * osi_mem_local_ctx_get(void)
{
    struct osi_mem_local_ctx_data * ldata;

    ldata = pthread_getspecific(osi_mem_local_pthread_key);

    /*
     * XXX this is an ugly hack until we force everyone to switch from
     * pthread_create to osi_thread_createU and register an osi_thread_event
     * to manage ctx local data structures
     */
    if (osi_compiler_expect_false(ldata == osi_NULL)) {
	osi_mem_local_ctx_init(0, &ldata);
    }
    return ldata;
}

#define osi_mem_local_ctx_put()

#define osi_mem_local_ctx_set(ldata) \
    (pthread_setspecific(osi_mem_local_pthread_key, (ldata)))


#endif /* _OSI_PTHREAD_UMEM_LOCAL_H */
