/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_OBJECT_INIT_H
#define _OSI_COMMON_OBJECT_INIT_H 1


typedef void osi_object_init_func_t(void);

#if defined(OSI_PTHREAD_ENV)
typedef struct {
    osi_fast_uint osi_volatile initialized;
    osi_object_init_func_t * fp;
    pthread_once_t once;
} osi_object_init_t;
#define OSI_OBJECT_INIT_DECL(sym, fp) \
    osi_object_init_t sym = { \
			      0, \
			      fp, \
			      PTHREAD_ONCE_INIT }
#elif defined(OSI_PREEMPTIVE_ENV)
#include <osi/osi_mutex.h>
osi_extern osi_mutex_t __osi_object_init_lock;
typedef struct {
    osi_fast_uint osi_volatile initialized;
    osi_object_init_func_t * fp;
} osi_object_init_t;
#define OSI_OBJECT_INIT_DECL(sym, fp) \
    osi_object_init_t sym = { \
			      0, \
			      fp }
#else /* !OSI_PREEMPTIVE_ENV */
typedef struct {
    osi_fast_uint initialized;
    osi_object_init_func_t * fp;
} osi_object_init_t;
#define OSI_OBJECT_INIT_DECL(sym, fp) \
    osi_object_init_t sym = { \
			      0, \
			      fp }
#endif /* !OSI_PREEMPTIVE_ENV */

#if defined(OSI_KERNELSPACE_ENV)
osi_extern osi_result osi_object_init_PkgInit(void);
osi_extern osi_result osi_object_init_PkgShutdown(void);
#else
#define osi_object_init_PkgInit()      (OSI_OK)
#define osi_object_init_PkgShutdown()  (OSI_OK)
#endif

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/COMMON/object_init_inline.h>
#endif

#endif /* _OSI_COMMON_OBJECT_INIT_H */
