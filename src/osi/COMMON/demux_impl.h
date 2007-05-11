/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_DEMUX_IMPL_H
#define _OSI_COMMON_DEMUX_IMPL_H 1

/*
 * osi
 * demultiplexer abstraction
 * internal type definitions
 */

#include <osi/osi_list.h>
#include <osi/osi_rwlock.h>

#define OSI_DEMUX_HASH_TABLE_SIZE 8 /* must be of the form 2^N */
#define OSI_DEMUX_HASH_TABLE_MASK (OSI_DEMUX_HASH_TABLE_SIZE-1)
#define OSI_DEMUX_HASH_TABLE_FUNC(x) ((x) & OSI_DEMUX_HASH_TABLE_MASK)

typedef struct {
    osi_list_head_volatile hash_chain;
    osi_rwlock_t chain_lock;
} osi_demux_hash_chain_t;

struct osi_demux {
    osi_demux_hash_chain_t hash_table[OSI_DEMUX_HASH_TABLE_SIZE];
    void * rock;
    osi_demux_options_t opts;
};

typedef struct {
    osi_list_element_volatile hash_chain;
    osi_uint32 action;
    void * action_rock;
    osi_demux_action_func_t * action_fp;
    osi_demux_action_options_t opts;
} osi_demux_action_t;

#endif /* _OSI_COMMON_DEMUX_IMPL_H */
