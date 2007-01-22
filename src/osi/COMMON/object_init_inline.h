/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_OBJECT_INIT_INLINE_H
#define _OSI_COMMON_OBJECT_INIT_INLINE_H 1

/*
 * common thread once inlines
 */

#include <osi/osi_atomic.h>

#if defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_STORES)
#define osi_object_init_membar_stores() osi_atomic_membar_order_stores()
#else
#define osi_object_init_membar_stores()
#endif

#if defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_LOADS)
#define osi_object_init_membar_loads() osi_atomic_membar_order_loads()
#else
#define osi_object_init_membar_loads()
#endif

#if defined(OSI_IMPLEMENTS_ATOMIC_CAS_FAST)
#if defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_STORES)
#if defined(OSI_IMPLEMENTS_ATOMIC_MEMBAR_ORDER_LOADS)
#define OSI_OBJECT_INIT_ATOMIC 1
#endif
#endif
#endif


#if defined(OSI_PREEMPTIVE_ENV)
#if defined(OSI_PTHREAD_ENV)
osi_inline_define(
void
osi_object_init(osi_object_init_t * once)
{
    if (osi_compiler_expect_false(!once->initialized)) {
	pthread_once(&once->once, once->fp);
	osi_object_init_membar_stores();
	once->initialized = 1;
    }
}
)
osi_inline_define(
int
osi_object_init_check(osi_object_init_t * once)
{
    return once->initialized;
}
)
#elif defined(OSI_OBJECT_INIT_ATOMIC)
osi_inline_define(
void
osi_object_init(osi_object_init_t * once)
{
    osi_fast_uint oval;

    if (osi_compiler_expect_true(once->initialized == 2)) {
	return;
    }

    oval = osi_atomic_cas_fast(&once->initialized,
			       0,
			       1);
    if (oval == 0) {
	(*once->fp)();
        osi_object_init_membar_stores();
	once->initialized = 2;
    } else {
	while (once->initialized == 1);
	osi_object_init_membar_loads();
    }
}
)
osi_inline_define(
int
osi_object_init_check(osi_object_init_t * once)
{
    return (once->initialized == 2);
}
)
#else /* !OSI_IMPLEMENTS_ATOMIC_CAS_FAST */
osi_inline_define(
void
osi_object_init(osi_object_init_t * once)
{
    osi_fast_uint oval;

    if (osi_compiler_expect_true(once->initialized == 1)) {
	return;
    }

    osi_mutex_Lock(&__osi_object_init_lock);
    if (once->initialized == 0) {
	(*once->fp)();
	osi_object_init_membar_stores();
	once->initialized = 1;
    }
    osi_mutex_Unlock(&__osi_object_init_lock);
}
)
osi_inline_define(
int
osi_object_init_check(osi_object_init_t * once)
{
    return once->initialized;
}
)
#endif /* !OSI_IMPLEMENTS_ATOMIC_CAS_FAST */
#else /* !OSI_PREEMPTIVE_ENV */
osi_inline_define(
void
osi_object_init(osi_object_init_t * once)
{
    if (osi_compiler_expect_false(!once->initialized)) {
	(*once->fp)();
	once->initialized = 1;
    }
}
)
osi_inline_define(
int
osi_object_init_check(osi_object_init_t * once)
{
    return once->initialized;
}
)
#endif /* !OSI_PREEMPTIVE_ENV */

osi_inline_define(
void
osi_object_init_assert(osi_object_init_t * once)
{
    osi_Assert(osi_object_init_check(once));
}
)

osi_inline_prototype(
void
osi_object_init(osi_object_init_t * once)
)
osi_inline_prototype(
int
osi_object_init_check(osi_object_init_t * once)
)
osi_inline_prototype(
void
osi_object_init_assert(osi_object_init_t * once)
)

#endif /* _OSI_COMMON_OBJECT_INIT_INLINE_H */
