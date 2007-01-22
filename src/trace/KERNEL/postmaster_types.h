/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_POSTMASTER_TYPES_H
#define _OSI_TRACE_KERNEL_POSTMASTER_TYPES_H 1


/*
 * osi tracing framework
 * inter-process asynchronous messaging
 * kernel postmaster
 * type definitions
 */

#include <osi/osi_mutex.h>
#include <osi/osi_condvar.h>
#include <osi/osi_list.h>
#include <trace/gen_rgy.h>
#include <trace/mail.h>

typedef enum {
    OSI_TRACE_MAILBOX_OPEN,
    OSI_TRACE_MAILBOX_SHUT,
    OSI_TRACE_MAILBOX_DESTROYED
} osi_trace_mailbox_state_t;

/* forward declare */
struct osi_trace_generator_registration;

typedef struct osi_trace_mailbox {
    osi_list_head_volatile messages;
    osi_list_head_volatile taps;
    osi_uint32 osi_volatile len;
    osi_trace_mailbox_state_t osi_volatile state;
    osi_trace_gen_id_t gen_id;
    struct osi_trace_generator_registration * gen;
    osi_uint32 osi_volatile tap_active;
    osi_mutex_t lock;
    osi_condvar_t cv;
} osi_trace_mailbox_t;

#endif /* _OSI_TRACE_KERNEL_POSTMASTER_TYPES_H */
