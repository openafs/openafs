/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_GEN_RGY_TYPES_H
#define _OSI_TRACE_KERNEL_GEN_RGY_TYPES_H 1


/*
 * osi tracing framework
 * generator registry
 * kernel support
 * type definitions
 */

#include <osi/osi_mutex.h>
#include <osi/osi_list.h>
#include <osi/osi_refcnt.h>
#include <trace/gen_rgy.h>
#include <trace/KERNEL/postmaster_types.h>

/* we can have a lower gen rgy id maximum in the kernel in order to save memory */
#define OSI_TRACE_GEN_RGY_MAX_ID_KERNEL 31   /* must be of the form (2^n)-1 */

typedef enum {
    OSI_TRACE_GEN_STATE_INVALID,
    OSI_TRACE_GEN_STATE_OFFLINE,
    OSI_TRACE_GEN_STATE_ONLINE,
    OSI_TRACE_GEN_STATE_MAX_ID
} osi_trace_generator_state_t;

typedef struct osi_trace_generator_registration {
    /*
     * BEGIN sync block
     * the following fields are protected by osi_trace_gen_rgy.lock
     */
    osi_list_element_volatile rgy_list;
    osi_list_element_volatile ptype_list;
    osi_trace_gen_id_t id;
    /*
     * END sync block
     */

    void * cache_padding0;

    /*
     * BEGIN sync block
     * the following fields are protected by osi_trace_gen_rgy.pid_hash[chain].lock
     */
    osi_list_element_volatile pid_hash;
    /*
     * END sync block
     */

    /*
     * BEGIN sync block
     * the following fields are IMMUTABLE once the object is registered
     */
    osi_trace_generator_address_t osi_volatile addr;
    /*
     * END sync block
     */

    /*
     * BEGIN sync block
     * the following fields are protected by gen->lock
     */
    osi_trace_generator_state_t osi_volatile state;
    osi_list_head_volatile holds;  /* refs held on other gens */
    /*
     * END sync block
     */

    osi_uint32 cache_padding1[5];

    /*
     * BEGIN sync block
     * the following fields are internally synchronized
     */
    osi_trace_mailbox_t mailbox;
    osi_refcnt_t refcnt;
    /*
     * END sync block
     */

    osi_mutex_t lock;
} osi_trace_generator_registration_t;

/*
 * struct to track holds of one gen
 * by another userspace gen
 */
typedef struct {
    osi_list_element_volatile hold_list;
    osi_trace_generator_registration_t * gen;
} osi_trace_gen_rgy_hold_t;

#endif /* _OSI_TRACE_KERNEL_GEN_RGY_TYPES_H */
