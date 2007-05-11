/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_OBJECT_INIT_H
#define _OSI_COMMON_OBJECT_INIT_H 1


typedef void osi_object_init_func_t(void);

#if defined(OSI_ENV_PTHREAD)
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
#elif defined(OSI_ENV_PREEMPTIVE)
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
#else /* !OSI_ENV_PREEMPTIVE */
typedef struct {
    osi_fast_uint initialized;
    osi_object_init_func_t * fp;
} osi_object_init_t;
#define OSI_OBJECT_INIT_DECL(sym, fp) \
    osi_object_init_t sym = { \
			      0, \
			      fp }
#endif /* !OSI_ENV_PREEMPTIVE */

#if defined(OSI_ENV_KERNELSPACE)
OSI_INIT_FUNC_PROTOTYPE(osi_object_init_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_object_init_PkgShutdown);
#else
#define osi_object_init_PkgInit       osi_null_init_func
#define osi_object_init_PkgShutdown   osi_null_fini_func
#endif

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/COMMON/object_init_inline.h>
#endif

#endif /* _OSI_COMMON_OBJECT_INIT_H */
