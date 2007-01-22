/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_LEGACY_REFCNT_H
#define	_OSI_OSI_LEGACY_REFCNT_H

/*
 * implementation of reference counting
 * using spinlocks
 */

#if !defined(OSI_IMPLEMENTS_REFCNT_ATOMIC)

#define OSI_IMPLEMENTS_REFCNT 1
#define OSI_IMPLEMENTS_LEGACY_REFCNT 1

#include <osi/osi_spinlock.h>


typedef osi_uint32 osi_refcnt_val_t;
typedef struct {
    osi_refcnt_val_t osi_volatile refcnt;
    osi_spinlock_t lock;
} osi_refcnt_t;

osi_extern void osi_refcnt_init(osi_refcnt_t *, osi_refcnt_val_t);
osi_extern void osi_refcnt_destroy(osi_refcnt_t *);

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/LEGACY/refcnt_inline.h>
#endif

#endif /* !OSI_IMPLEMENTS_REFCNT_ATOMIC */

#endif /* _OSI_OSI_LEGACY_REFCNT_H */
