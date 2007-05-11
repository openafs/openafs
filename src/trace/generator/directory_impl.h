/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_PROBE_DIRECTORY_IMPL_H
#define _OSI_TRACE_GENERATOR_PROBE_DIRECTORY_IMPL_H 1

/*
 * osi tracing framework
 * trace data generator library
 * probe point directory
 */

#include <osi/osi_list.h>
#include <osi/osi_rwlock.h>
#include <trace/generator/probe.h>
#include <osi/osi_atomic.h>
#include <trace/generator/directory.h>
#include <trace/generator/activation_impl.h>

/*
 * probe directory main data structures
 *
 * the trace directory is implemented as a forest
 * the "root" of the forest and all parent nodes
 * within individual trees are implemented as
 * hash tables.
 *
 * for example, take the probe "rpc.rx.packet.alloc"
 * "rpc" would be a tree root. "rx" would form a subtree
 * of "rpc". likewise, "packet" would form a subtree
 * of "rx".  And, "alloc" would be a leaf node of "packet"
 */

/* max number of discrete tokens into which a probe name can legally be broken */
#define OSI_TRACE_PROBE_TOKEN_LEN_MAX 32

/* defines the number of hash backets in a parent node in the forest */
#define OSI_TRACE_PROBE_NAME_HASH_SIZE 7   /* should be a prime number */
#define OSI_TRACE_PROBE_NAME_HASH_FUNC(hash) (hash % OSI_TRACE_PROBE_NAME_HASH_SIZE)

typedef enum {
    OSI_TRACE_DIRECTORY_NODE_ROOT,
    OSI_TRACE_DIRECTORY_NODE_INTERMEDIATE,
    OSI_TRACE_DIRECTORY_NODE_LEAF
} osi_trace_directory_node_type_t;

/* common header on all tree element types */
struct osi_trace_directory_node {
    osi_trace_directory_node_type_t osi_volatile node_type;
    osi_uint32 osi_volatile probe_name_hash;
    osi_list_element_volatile hash_chain;
    struct osi_trace_directory_intermediate_node * osi_volatile parent;
    char probe_name[OSI_TRACE_PROBE_TOKEN_LEN_MAX];
};

struct osi_trace_directory_leaf_node {
    struct osi_trace_directory_node hdr;
    osi_trace_probe_t probe;
};

struct osi_trace_directory_intermediate_node {
    struct osi_trace_directory_node hdr;
    osi_list_head_volatile hash_chains[OSI_TRACE_PROBE_NAME_HASH_SIZE];
};


/* tree node pointer union */
typedef union {
    struct osi_trace_directory_node * osi_volatile node;
    struct osi_trace_directory_leaf_node * osi_volatile leaf;
    struct osi_trace_directory_intermediate_node * osi_volatile intermed;
} osi_trace_directory_node_ptr_t;


/*
 * the main directory structure
 *
 * contains the hash table of tree roots, and
 * the vector of all registered probes
 */
struct osi_trace_directory {
    osi_uint32 magic;
    struct osi_trace_directory_intermediate_node root;
    struct osi_trace_directory_leaf_node * osi_volatile probes[OSI_TRACE_PROBES_MAX];
    osi_trace_probe_id_t osi_volatile id_counter;
    osi_rwlock_t lock;
};


/*
 * structures used during forest searchs
 */
struct osi_trace_directory_token_stack {
    const char * tokens[OSI_TRACE_MAX_PROBE_TREE_DEPTH*2+1];
    osi_uint8 bound[OSI_TRACE_MAX_PROBE_TREE_DEPTH];
    int depth;
};

struct osi_trace_directory_child_walker {
    osi_trace_directory_node_ptr_t ptr;
    osi_uint8 idx;
    osi_uint8 min_tok_idx;
    osi_uint8 max_tok_idx;
    osi_uint8 traverse_all : 1;
    osi_uint8 telescoping_wildcard : 1;
    osi_uint8 initialized : 1;
};

struct osi_trace_directory_tree_walker {
    struct osi_trace_directory_child_walker stack[OSI_TRACE_MAX_PROBE_TREE_DEPTH+1];
    struct osi_trace_directory_token_stack tok;
    int depth;
    struct {
	osi_trace_directory_foreach_action_t * fp;
	void * data;
    } action;
};


/*
 * register a new probe id
 *
 * [OUT] id -- new probe id
 */
osi_extern osi_result osi_trace_directory_probe_id_alloc(osi_trace_probe_id_t *);
#if defined(OSI_IMPLEMENTS_ATOMIC_INC_32_NV)
#define OSI_TRACE_DIRECTORY_ATOMIC_PROBE_ID_ALLOC 1
#endif

osi_extern osi_result osi_trace_directory_probe_id_max(osi_trace_probe_id_t *);

#endif /* _OSI_TRACE_GENERATOR_PROBE_DIRECTORY_IMPL_H */
