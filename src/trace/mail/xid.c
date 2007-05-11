/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi tracing framework
 * inter-process asynchronous messaging system
 * transaction id subsystem
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_cache.h>
#include <osi/osi_atomic.h>
#include <osi/osi_mutex.h>
#include <trace/mail.h>
#include <trace/mail/xid.h>

#include <osi/osi_lib_init.h>

osi_lib_init_prototype(__osi_trace_mail_xid_init);

osi_extern osi_init_func_t * __osi_trace_mail_xid_init_ptr;
osi_extern osi_fini_func_t * __osi_trace_mail_xid_fini_ptr;

osi_lib_init_decl(__osi_trace_mail_xid_init)
{
    __osi_trace_mail_xid_init_ptr = &osi_trace_mail_xid_PkgInit;
    __osi_trace_mail_xid_fini_ptr = &osi_trace_mail_xid_PkgShutdown;
}

/* 
 * XXX 
 * still need to deal with cases where osi_uint64 is an afs_hyper_t 
 */

#ifdef AFS_64BIT_ENV
#define OSI_TRACE_MAIL_64BIT_XID_NAMESPACE 1
#if defined(OSI_IMPLEMENTS_ATOMIC_INC_64_NV)
#define OSI_TRACE_MAIL_ATOMIC_XID_ALLOC 1
#define osi_trace_mail_xid_inc(x,v)  (*v = osi_atomic_inc_64_nv(x))
#endif /* OSI_IMPLEMENTS_ATOMIC_INC_64_NV */
#else /* !AFS_64BIT_ENV */
#if defined(OSI_IMPLEMENTS_ATOMIC_INC_32_NV)
#define OSI_TRACE_MAIL_ATOMIC_XID_ALLOC 1
#define osi_trace_mail_xid_inc(x, v) \
    osi_Macro_Begin \
 osi_atomic_inc_32_nv(x)
#endif /* OSI_IMPLEMENTS_ATOMIC_INC_32_NV */
#endif /* !AFS_64BIT_ENV */

struct osi_trace_mail_xid_allocator {
#ifdef OSI_TRACE_MAIL_64BIT_XID_NAMESPACE
    osi_volatile osi_uint64 xid_counter;
#else
    osi_volatile osi_uint32 xid_counter;
#endif
#ifndef OSI_TRACE_MAIL_ATOMIC_XID_ALLOC
    osi_mutex_t xid_lock;
#endif
};

osi_static struct osi_trace_mail_xid_allocator osi_trace_mail_xid_allocator;

/*
 * allocate a transaction id
 *
 * [INOUT] xid  -- address in which to store new transaction id
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_mail_xid_alloc(osi_trace_mail_xid_t * xid)
{
#ifdef OSI_TRACE_MAIL_ATOMIC_XID_ALLOC
    *xid = osi_atomic_inc_64_nv(&osi_trace_mail_xid_allocator.xid_counter);
#else
    osi_mutex_Lock(&osi_trace_mail_xid_allocator.xid_lock);
    *xid = osi_trace_mail_xid_allocator.xid_counter++;
    osi_mutex_Unlock(&osi_trace_mail_xid_allocator.xid_lock);
#endif
    return OSI_OK;
}

/*
 * retire a previously allocated transaction id
 *
 * [IN] xid -- transaction id to retire
 *
 * preconditions:
 *   valid xid
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_mail_xid_retire(osi_trace_mail_xid_t xid)
{
    return OSI_OK;
}

osi_result
osi_trace_mail_xid_PkgInit(void)
{
#ifndef OSI_TRACE_MAIL_ATOMIC_XID_ALLOC
    osi_mutex_options_t opt;

    /* XXX we should eventually use osi_trace_common_options,
     * but first we need an API to do options copying */
    osi_mutex_options_Init(&opt);
    osi_mutex_options_Set(&opt,
			  OSI_MUTEX_OPTION_PREEMPTIVE_ONLY,
			  1);
    osi_mutex_options_Set(&opt,
			  OSI_MUTEX_OPTION_TRACE_ALLOWED,
			  0);
    osi_mutex_Init(&osi_trace_mail_xid_allocator.xid_lock, &opt);
    osi_mutex_options_Destroy(&opt);
#endif

    osi_trace_mail_xid_allocator.xid_counter = 1;

    return OSI_OK;
}

osi_result
osi_trace_mail_xid_PkgShutdown(void)
{
#ifndef OSI_TRACE_MAIL_ATOMIC_XID_ALLOC
    osi_mutex_Destroy(&osi_trace_mail_xid_allocator.xid_lock);
#endif
    return OSI_OK;
}
