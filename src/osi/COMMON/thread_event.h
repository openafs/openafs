/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_THREAD_EVENT_H
#define _OSI_COMMON_THREAD_EVENT_H 1

/*
 * common thread event handler support
 */

#include <osi/osi_list.h>

/* use powers of 2 so we can do bitmasks */
typedef enum {
    OSI_THREAD_EVENT_CREATE = 1,
    OSI_THREAD_EVENT_DESTROY = 2,
    OSI_THREAD_EVENT_ALL = 3
} osi_thread_event_type_t;

typedef osi_result osi_thread_event_handler_t(osi_thread_id_t, osi_thread_event_type_t, void *);
typedef struct {
    osi_list_element_volatile create_event_list;
    osi_list_element_volatile destroy_event_list;
    osi_thread_event_handler_t * osi_volatile fp;
    void * osi_volatile sdata;
    osi_uint32 osi_volatile events_mask;
} osi_thread_event_t;


OSI_INIT_FUNC_PROTOTYPE(osi_thread_event_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_thread_event_PkgShutdown);

osi_extern void * osi_thread_run(void * arg);

osi_extern osi_result osi_thread_event_create(osi_thread_event_t **, 
					      osi_thread_event_handler_t *,
					      void *);
osi_extern osi_result osi_thread_event_destroy(osi_thread_event_t *);

osi_extern osi_result osi_thread_event_register(osi_thread_event_t *, osi_uint32 mask);
osi_extern osi_result osi_thread_event_unregister(osi_thread_event_t *, osi_uint32 mask);

#endif /* _OSI_COMMON_THREAD_EVENT_H */
