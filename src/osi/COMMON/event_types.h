/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_EVENT_TYPES_H
#define _OSI_COMMON_EVENT_TYPES_H 1

/*
 * osi event action framework
 *
 * implementation using traditional synchronization primitives
 */

#include <osi/osi_condvar.h>
#include <osi/osi_mutex.h>

typedef osi_uint32 osi_event_id_t;

/* forward declare */
struct osi_event_subscription;
struct osi_event_hook;

/* action types */
typedef osi_result osi_event_action_func_t(osi_event_id_t,
					   void * event_sdata,
					   struct osi_event_subscription *,
					   void * sub_sdata,
					   struct osi_event_hook *,
					   void * hook_sdata);

typedef struct osi_event_action_cv {
    osi_mutex_t * osi_volatile lock;
    osi_condvar_t * osi_volatile cv;
    osi_uint32 osi_volatile broadcast;
} osi_event_action_cv_t;

typedef struct osi_event_action_queue {
    osi_mutex_t * osi_volatile lock;
    osi_condvar_t * osi_volatile cv;
    osi_list_head_volatile * osi_volatile list;
} osi_event_action_queue_t;



typedef enum {
    OSI_EVENT_ACTION_NONE,        /* no-op */
    OSI_EVENT_ACTION_FUNCTION,    /* fire a function pointer */
    OSI_EVENT_ACTION_CV,          /* signal or broadcast a cv */
    OSI_EVENT_ACTION_QUEUE,       /* enqueue an osi_event_record_t */
    OSI_EVENT_ACTION_MAX_ID
} osi_event_action_type_t;

typedef union {
    void * osi_volatile opaque;
    osi_event_action_func_t * osi_volatile func;
    osi_event_action_cv_t * osi_volatile cv;
    osi_event_action_queue_t * osi_volatile queue;
} osi_event_action_ptr_t;

typedef struct osi_event_action {
    osi_event_action_ptr_t data;
    osi_event_action_type_t osi_volatile type;
} osi_event_action_t;


typedef struct osi_event_record {
    osi_list_element_volatile record_queue;
    osi_event_id_t osi_volatile event_id;
    void * osi_volatile event_sdata;
    struct osi_event_subscription * osi_volatile sub;
    void * osi_volatile sub_sdata;
    struct osi_event_hook * osi_volatile hook;
    void * osi_volatile hook_sdata;
} osi_event_record_t;

#endif /* _OSI_COMMON_EVENT_TYPES_H */
