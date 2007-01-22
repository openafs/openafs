/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_THREAD_EVENT_IMPL_H
#define _OSI_COMMON_THREAD_EVENT_IMPL_H 1

/*
 * common thread event handler implementation
 */

#include <osi/osi_list.h>
#include <osi/osi_rwlock.h>

struct osi_thread_run_arg {
    void * (*fp)(void *);
    void * arg;
    size_t arg_len;
    osi_thread_options_t opt;
};

struct osi_thread_event_table {
    osi_list_head_volatile create_events;
    osi_list_head_volatile destroy_events;
    osi_rwlock_t lock;
};

osi_extern osi_result __osi_thread_run_args_alloc(struct osi_thread_run_arg **);
osi_extern osi_result __osi_thread_run_args_free(struct osi_thread_run_arg *);

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/COMMON/thread_event_inline.h>
#endif

#endif /* _OSI_COMMON_THREAD_EVENT_IMPL_H */
