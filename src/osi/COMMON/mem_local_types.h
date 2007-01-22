/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_MEM_LOCAL_TYPES_H
#define _OSI_COMMON_MEM_LOCAL_TYPES_H 1


typedef osi_uint32 osi_mem_local_key_t;


/* 
 * in kernelspace, we want cpu-local variables indexed by cpu id.
 * in userspace, we want thread-local variables indexed by thread id.
 */
#if defined(OSI_KERNELSPACE_ENV)
#include <osi/osi_cpu.h>
typedef osi_cpu_id_t osi_mem_local_ctx_id_t;
#else
#include <osi/osi_thread.h>
typedef osi_thread_id_t osi_mem_local_ctx_id_t;
#endif


/*
 * each mem_local "context" (a cpu in kernelspace, a thread in userspace)
 * has its own local buffer which contains the key to pointer mappings,
 * and a buffer which can be allocated to small, frequently used
 * context-local data structures.  This is done to improve cache and TLB
 * utilization.
 */

#define OSI_MEM_LOCAL_MAX_KEYS 64          /* allow 64 separately addressable per-cpu structs */
#define OSI_MEM_LOCAL_BUFFER_SIZE 16384    /* allocate 16kb per cpu in the main per-cpu buffer */

#define OSI_MEM_LOCAL_PAYLOAD_OFFSET ((OSI_MEM_LOCAL_MAX_KEYS * sizeof(void *)) + \
                                      sizeof(struct osi_mem_local_ctx_header))
#define OSI_MEM_LOCAL_PAYLOAD_SIZE   (OSI_MEM_LOCAL_BUFFER_SIZE - OSI_MEM_LOCAL_PAYLOAD_OFFSET)


#ifdef OSI_USERSPACE_ENV
#include <osi/osi_list.h>
#endif

struct osi_mem_local_ctx_header {
#ifdef OSI_USERSPACE_ENV
    osi_list_element_volatile ctx_list;
#endif
    osi_mem_local_ctx_id_t ctx_id;
};

typedef struct osi_mem_local_ctx_data {
    struct osi_mem_local_ctx_header hdr;
    void * keys[OSI_MEM_LOCAL_MAX_KEYS];
    osi_uint8 payload[OSI_MEM_LOCAL_PAYLOAD_SIZE];
} osi_mem_local_ctx_data_t;


#endif /* _OSI_COMMON_MEM_LOCAL_TYPES_H */
