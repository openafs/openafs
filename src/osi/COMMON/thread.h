/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_THREAD_H
#define _OSI_COMMON_THREAD_H 1

/*
 * osi abstraction
 * thread api
 * common support
 */

typedef void * osi_thread_func_t(void *);

OSI_INIT_FUNC_PROTOTYPE(osi_thread_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_thread_PkgShutdown);


#define OSI_THREAD_FUNC_DECL(sym) \
    void * \
    sym (void * arg)
#define OSI_THREAD_FUNC_PROTOTYPE(sym) \
    osi_extern void * sym (void *)
#define OSI_THREAD_FUNC_STATIC_DECL(sym) \
    osi_static void * \
    sym (void * arg)
#define OSI_THREAD_FUNC_STATIC_PROTOTYPE(sym) \
    osi_static void * sym (void *)


#endif /* _OSI_COMMON_THREAD_H */
