/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


/*
 * osi tracing framework
 * trace data generator library
 * probe point directory
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <trace/generator/activation.h>
#include <trace/generator/directory.h>
#include <trace/generator/directory_impl.h>
#include <trace/generator/directory_mail.h>
#include <osi/osi_mem.h>
#include <osi/osi_atomic.h>
#include <osi/osi_string.h>

struct osi_trace_directory osi_trace_directory;

/*
 * use a combination of xor and bit shifts
 * to build a hash from the string
 */
osi_static osi_inline osi_uint32
osi_trace_probe_name_hash(const char * probe_name)
{
    int pos, count = 0;
    osi_uint32 hash = 0, tmp = 0;

    pos = osi_string_len(probe_name) - 1;
    for( ; pos >= 0 ; pos--, count++) {
	if (count == 4) {
	    hash ^= tmp;
	    hash ^= (hash << 3) ^ (hash >> 5);
	    count = 0;
	    tmp = 0;
	}
	tmp <<= 7;
	tmp |= (osi_uint32) probe_name[pos];
    }

    if (tmp) {
	hash ^= tmp;
	hash ^= (hash << 3) ^ (hash >> 5);
    }

    return hash;
}

/*
 * simple strtok_r equivalent
 */
struct osi_trace_probe_name_extractor {
    const char * probe_name;
    const char * cur_ptr;
    const char * end;
    osi_uint32 done;
    char scratch[OSI_TRACE_MAX_PROBE_NAME_LEN];
};

osi_static osi_inline void
osi_trace_probe_name_extract_begin(struct osi_trace_probe_name_extractor * ext, 
				   const char * probe_name)
{
    ext->probe_name = probe_name;
    ext->cur_ptr = ext->scratch;
    ext->end = ext->scratch + sizeof(ext->scratch);
    osi_string_ncpy(ext->scratch, probe_name, sizeof(ext->scratch));
    ext->done = 0;
}

osi_static osi_inline char *
osi_trace_probe_name_extract_next_token(struct osi_trace_probe_name_extractor * ext)
{
    int i;
    char * cur, * ret;

    if (ext->done) {
        ret = osi_NULL;
        goto done;
    }

    /* find the next separator */
    for (cur = ret = (char *) ext->cur_ptr;
	 cur != ext->end && cur[0] != '\0' && cur[0] != '.';
	 ext->cur_ptr++, cur++);

    /* check whether we're out of tokens */
    if ((cur == ext->end) || (cur[0] == '\0')) {
        ext->done = 1;
    }

    /* advance beyond the separator */
    ext->cur_ptr++;

    if (cur[0] == '.') {
	cur[0] = '\0';
    }

 done:
    return ret;
}

/*
 * token stack library
 * (turns a probe name into a stack of tokens,
 *  also performs wildcard optimizing transforms)
 */
osi_static osi_result
osi_trace_directory_token_stack_build(const char * probe_name,
				      struct osi_trace_probe_name_extractor * ext,
				      struct osi_trace_directory_token_stack * stack)
{
    osi_result res = OSI_OK;
    const char * tok;

    osi_trace_probe_name_extract_begin(ext, probe_name);

    stack->depth = 0;
    for (tok = osi_trace_probe_name_extract_next_token(ext);
	 tok;
	 tok = osi_trace_probe_name_extract_next_token(ext)) {
	if (!osi_string_cmp(tok, "+")) {
	    stack->tokens[stack->depth++] = "?";
	    stack->tokens[stack->depth++] = "*";
	} else {
	    stack->tokens[stack->depth++] = tok;
	}
    }
    stack->tokens[stack->depth] = osi_NULL;

    return res;
}

osi_static osi_inline void
osi_trace_directory_token_stack_delete(struct osi_trace_directory_token_stack * stack,
				       int idx)
{
    for ( ; idx < stack->depth; idx++ ) {
	stack->tokens[idx] = stack->tokens[idx+1];
    }
    stack->depth--;
}

osi_static osi_inline void
osi_trace_directory_token_stack_swap(struct osi_trace_directory_token_stack * stack,
				       int i, int j)
{
    const char * tmp = stack->tokens[i];
    stack->tokens[i] = stack->tokens[j];
    stack->tokens[j] = tmp;
}

osi_static osi_result
osi_trace_directory_token_stack_optimize(struct osi_trace_directory_token_stack * stack)
{
    osi_result res = OSI_OK;
    int i, pass, changed;

    /* allow up to three passes of optimization */
    for (pass = 0; pass < 3; pass++) {
	changed = 0;
	for (i = 0; i < stack->depth; i++) {
	    if ((stack->tokens[i] != osi_NULL) &&
		(stack->tokens[i+1] != osi_NULL)) {
		if (!osi_string_cmp(stack->tokens[i], "*")) {
		    if (!osi_string_cmp(stack->tokens[i+1], "*")) {
			osi_trace_directory_token_stack_delete(stack, i+1);
		    } else if (!osi_string_cmp(stack->tokens[i+1], "?")) {
			changed = 1;
			osi_trace_directory_token_stack_swap(stack, i, i+1);
		    }
		}
	    }
	}
	if (!changed) {
	    /* if no _substantial_ changes occurred, break out */
	    break;
	}
    }

    return res;
}

osi_static osi_result
osi_trace_directory_token_stack_verify(struct osi_trace_directory_token_stack * stack)
{
    osi_result res = OSI_OK;
    int i;
    int wildcards = 0;
    int non_telescoping = 0;

    for (i = 0; i < stack->depth; i++) {
	if (osi_compiler_expect_false(stack->tokens[i] == osi_NULL)) {
	    res = OSI_FAIL;
	    break;
	}
	if (!osi_string_cmp(stack->tokens[i], "*")) {
	} else {
	    non_telescoping++;
	}
    }

    if (osi_compiler_expect_false(non_telescoping > OSI_TRACE_MAX_PROBE_TREE_DEPTH)) {
	res = OSI_TRACE_DIRECTORY_ERROR_PROBE_FILTER_INVALID;
    }

    return res;
}

osi_static osi_result
osi_trace_directory_token_stack_eval(struct osi_trace_directory_tree_walker * w)
{
    osi_result res = OSI_FAIL;
    osi_trace_directory_node_ptr_t n;
    int tok, depth;

    depth = w->depth;
    n.node = w->stack[depth].ptr.node;
    tok = w->tok.depth - 1;

    if (osi_compiler_expect_false(n.node->node_type != OSI_TRACE_DIRECTORY_NODE_LEAF)) {
	goto error;
    }

    while (n.node != osi_NULL) {
	if (!osi_string_cmp(w->tok.tokens[tok], "*")) {
	    /* telescoping wildcard */
	    if ((tok == 0) ||
		w->tok.bound[tok-1]) {
		res = OSI_OK;
		break;
	    } else {
		/* XXX implement me */
	    }
	} else if (!osi_string_cmp(w->tok.tokens[tok], "?")) {
	    /* non-telescoping wildcard */
	    w->stack[depth].max_tok_idx = tok;
	    tok--;
	    depth--;
	    n.node = w->stack[depth].ptr.node;
	} else {
	    /* non-wildcard */
	    if (!osi_string_cmp(w->tok.tokens[tok], n.node->probe_name)) {
		w->stack[depth].max_tok_idx = tok;
		tok--;
		depth--;
		n.node = w->stack[depth].ptr.node;
	    } else {
		/* not a match */
		break;
	    }
	}
    }

 error:
    return res;
}


/* search an intermediate node for a specific child */
osi_static osi_result
osi_trace_directory_search_token(osi_trace_directory_node_ptr_t p,
				 const char * tok,
				 osi_trace_directory_node_ptr_t * node)
{
    osi_result res = OSI_FAIL;
    osi_trace_directory_node_ptr_t n;
    osi_uint32 hash, idx;

    /* 
     * search_token should never be called against a leaf node
     */
    osi_Assert(p.node->node_type != OSI_TRACE_DIRECTORY_NODE_LEAF);

    /* calculate hash chain for next probe name token */
    hash = osi_trace_probe_name_hash(tok);
    idx = OSI_TRACE_PROBE_NAME_HASH_FUNC(hash);

    /* scan all the children looking for this token */
    for (osi_list_Scan_Immutable(&p.intermed->hash_chains[idx],
				 n.node,
				 struct osi_trace_directory_node, 
				 hash_chain)) {
	if ((n.node->probe_name_hash == hash) &&
	    !osi_string_cmp(tok, n.node->probe_name)) {
	    node->node = n.node;
	    res = OSI_OK;
	    break;
	}
    }
    return res;
}

/*
 * tree walking interfaces
 */

osi_static osi_result
osi_trace_directory_walk_first_child(struct osi_trace_directory_tree_walker * w)
{
    osi_result res = OSI_OK;

    if (osi_compiler_expect_false(w->stack[w->depth].idx >= OSI_TRACE_PROBE_NAME_HASH_SIZE)) {
	res = OSI_FAIL;
	goto error;
    }

    while (osi_list_IsEmpty(&w->stack[w->depth].ptr.intermed->hash_chains[w->stack[w->depth].idx])) {
	w->stack[w->depth].idx++;
	if (w->stack[w->depth].idx == OSI_TRACE_PROBE_NAME_HASH_SIZE) {
	    res = OSI_FAIL;
	    goto error;
	}
    }

    w->stack[w->depth+1].ptr.node =
	osi_list_First(&w->stack[w->depth].ptr.intermed->hash_chains[w->stack[w->depth].idx],
		       struct osi_trace_directory_node,
		       hash_chain);

 error:
    return res;
}

osi_static osi_result
osi_trace_directory_walk_next_child(struct osi_trace_directory_tree_walker * w)
{
    osi_result res = OSI_OK;

    if (osi_list_IsLast(&w->stack[w->depth].ptr.intermed->hash_chains[w->stack[w->depth].idx],
			w->stack[w->depth+1].ptr.node,
			struct osi_trace_directory_node,
			hash_chain)) {
	w->stack[w->depth].idx++;
	res = osi_trace_directory_walk_first_child(w);
    } else {
	w->stack[w->depth+1].ptr.node =
	    osi_list_Next(w->stack[w->depth+1].ptr.node,
			  struct osi_trace_directory_node,
			  hash_chain);
    }

    return res;
}

osi_static osi_result
osi_trace_directory_walk_begin(struct osi_trace_directory_tree_walker * w,
			       struct osi_trace_probe_name_extractor * ext,
			       const char * filter,
			       osi_trace_directory_foreach_action_t * fp,
			       void * data)
{
    osi_result res = OSI_OK;
    const char * tok;

    osi_mem_zero(w, sizeof(struct osi_trace_directory_tree_walker));

    w->stack[0].ptr.intermed = &osi_trace_directory.root;
    w->action.fp = fp;
    w->action.data = data;

    res = osi_trace_directory_token_stack_build(filter, ext, &w->tok);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_directory_token_stack_optimize(&w->tok);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_directory_token_stack_verify(&w->tok);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    /* compute binding and traversal parameters for top of forest */
    tok = w->tok.tokens[0];
    if (osi_compiler_expect_false(tok == osi_NULL)) {
	res = OSI_FAIL;
    } else if (!osi_string_cmp(tok, "*") ||
	       !osi_string_cmp(tok, "+")) {
	/* telescoping wildcard */
	w->stack[0].traverse_all = 1;
	w->stack[0].telescoping_wildcard = 1;
    } else if (!osi_string_cmp(tok, "?")) {
	w->stack[0].traverse_all = 1;
    } else {
	w->tok.bound[0] = 1;
    }

 error:
    return res;
}

osi_static osi_result
osi_trace_directory_walk_recurse(struct osi_trace_directory_tree_walker * w)
{
    osi_result res = OSI_OK;
    const char * tok;
    int depth;

    depth = w->depth + 1;
    tok = w->tok.tokens[w->stack[depth-1].min_tok_idx];
    if (osi_compiler_expect_false(tok == osi_NULL)) {
	res = OSI_FAIL;
    } else if (!osi_string_cmp(tok, "*") ||
	       !osi_string_cmp(tok, "+")) {
	/* telescoping wildcard */
	w->stack[depth].traverse_all = 1;
	w->stack[depth].idx = 0;
	w->stack[depth].min_tok_idx = w->stack[depth-1].min_tok_idx;
	w->stack[depth].telescoping_wildcard = 1;
	res = osi_trace_directory_walk_first_child(w);
    } else if (!osi_string_cmp(tok, "?")) {
	/* non-telescoping wildcard */
	w->stack[depth].traverse_all = 1;
	w->stack[depth].idx = 0;
	w->stack[depth].min_tok_idx = w->stack[depth-1].min_tok_idx + 1;
	w->stack[depth].telescoping_wildcard = w->stack[depth-1].telescoping_wildcard;
	res = osi_trace_directory_walk_first_child(w);
    } else {
	/* non-wildcard */
	w->tok.bound[depth-1] = 1;
	w->stack[depth].traverse_all = 0;
	w->stack[depth].min_tok_idx = w->stack[depth-1].min_tok_idx + 1;
	w->stack[depth].telescoping_wildcard = w->stack[depth-1].telescoping_wildcard;
	res = osi_trace_directory_search_token(w->stack[depth-1].ptr,
					       tok,
					       &w->stack[depth].ptr);
    }
    w->depth++;

    return res;
}

osi_static osi_result
osi_trace_directory_walk_up(struct osi_trace_directory_tree_walker * w)
{
    osi_result res = OSI_OK;

    if (osi_compiler_expect_true(w->depth > 0)) {
	w->depth--;
    } else {
	res = OSI_FAIL;
    }

    return res;
}

osi_static osi_result
osi_trace_directory_walk_leaf(struct osi_trace_directory_tree_walker * w)
{
    osi_result res = OSI_OK;
    int i;
    const char * tok;

    if (w->stack[w->depth].telescoping_wildcard) {
	res = osi_trace_directory_token_stack_eval(w);
	if (OSI_RESULT_OK(res)) {
	    res = (*w->action.fp)(w->stack[w->depth].ptr.leaf->probe,
				  w->action.data);
	}
    } else {
	res = (*w->action.fp)(w->stack[w->depth].ptr.leaf->probe,
			      w->action.data);
    }

    return res;
}

/* main driver for walking the tree */
osi_static osi_result
osi_trace_directory_walk(struct osi_trace_directory_tree_walker * w)
{
    osi_result res = OSI_OK;

    while (1) {
	/* walk down to a leaf */
	while (w->stack[w->depth].ptr.node->node_type != OSI_TRACE_DIRECTORY_NODE_LEAF) {
	    res = osi_trace_directory_walk_recurse(w);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto done;
	    }
	}

	/* process the leaf */
	osi_trace_directory_walk_leaf(w);

	/* walk across the tree */
	do {
	    res = osi_trace_directory_walk_up(w);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		/* done processing entire tree; return success */
		res = OSI_OK;
		goto done;
	    }
	    if (w->stack[w->depth].traverse_all) {
		res = osi_trace_directory_walk_next_child(w);
	    } else {
		/* keep walking up until we hit a traverse_all node or the root */
		res = OSI_FAIL;
		continue;
	    }
	} while (OSI_RESULT_FAIL(res));
	    
    }

 done:
    return res;
}

/* add an intermediate node to the tree */
osi_static osi_result
osi_trace_directory_add_intermediate_node(struct osi_trace_directory_intermediate_node * parent,
					  struct osi_trace_directory_intermediate_node ** new_child,
					  const char * tok)
{
    osi_result res = OSI_OK;
    struct osi_trace_directory_intermediate_node * child;
    osi_uint32 i, hash, idx;

    /* calculate hash chain for next probe name token */
    hash = osi_trace_probe_name_hash(tok);
    idx = OSI_TRACE_PROBE_NAME_HASH_FUNC(hash);

    child = (struct osi_trace_directory_intermediate_node *)
	osi_mem_alloc_nosleep(sizeof(struct osi_trace_directory_intermediate_node));
    if (osi_compiler_expect_false(child == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    child->hdr.node_type = OSI_TRACE_DIRECTORY_NODE_INTERMEDIATE;
    child->hdr.parent = parent;
    child->hdr.probe_name_hash = hash;
    osi_string_ncpy(child->hdr.probe_name, tok, sizeof(child->hdr.probe_name));
    child->hdr.probe_name[sizeof(child->hdr.probe_name)-1] = '\0';

    for (i = 0; i < OSI_TRACE_PROBE_NAME_HASH_SIZE; i++) {
	osi_list_Init(&child->hash_chains[i]);
    }

    osi_list_Append(&parent->hash_chains[idx], child,
		    struct osi_trace_directory_intermediate_node, 
		    hdr.hash_chain);

    *new_child = child;

 error:
    return res;
}

/* add a leaf node to the tree */
osi_static osi_result
osi_trace_directory_add_leaf_node(struct osi_trace_directory_intermediate_node * parent,
				  struct osi_trace_directory_leaf_node ** new_child,
				  const char * tok,
				  osi_trace_probe_t probe)
{
    osi_result res = OSI_OK;
    struct osi_trace_directory_leaf_node * child;
    osi_uint32 hash, idx;

    /* calculate hash chain for next probe name token */
    hash = osi_trace_probe_name_hash(tok);
    idx = OSI_TRACE_PROBE_NAME_HASH_FUNC(hash);

    child = (struct osi_trace_directory_leaf_node *)
	osi_mem_alloc_nosleep(sizeof(struct osi_trace_directory_leaf_node));
    if (osi_compiler_expect_false(child == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    child->hdr.node_type = OSI_TRACE_DIRECTORY_NODE_LEAF;
    child->hdr.parent = parent;
    child->hdr.probe_name_hash = hash;
    osi_string_ncpy(child->hdr.probe_name, tok, sizeof(child->hdr.probe_name));
    child->hdr.probe_name[sizeof(child->hdr.probe_name)-1] = '\0';
    child->probe = probe;

    osi_list_Append(&parent->hash_chains[idx], child,
		    struct osi_trace_directory_leaf_node,
		    hdr.hash_chain);

    *new_child = child;

 error:
    return res;
}

/*
 * regiser an alias for a probe
 */
osi_static osi_result
osi_trace_directory_probe_register_internal(const char * probe_name, 
					    osi_trace_probe_t probe,
					    int primary_flag)
{
    osi_result res = OSI_FAIL;
    osi_trace_directory_node_ptr_t n;
    osi_trace_probe_id_t probe_id;
    struct osi_trace_probe_name_extractor * ext;
    const char *tok, *ntok;
    int found;
#if !defined(OSI_SMALLSTACK_ENV)
    struct osi_trace_probe_name_extractor ext_l;
#endif

    /*
     * algorithm needs to proceed as follows
     *
     * first:
     *  get the next (in this case first) token
     *  calculate the hash index
     *  load in the list head for the hash index from the root of the forest
     *  traverse the list until we get a complete hash match and string match
     *  goto next
     *
     * next:
     *  get the next token
     *  terminate on null token
     *  calculate the hash index
     *  load in the list head for the hash index from the parent node
     *  traverse the list until we get a complete hash match and string match
     *  goto next
     */

#if defined(OSI_SMALLSTACK_ENV)
    ext = (struct osi_trace_probe_name_extractor *)
	osi_mem_alloc_nosleep(sizeof(struct osi_trace_probe_name_extractor));
    if (osi_compiler_expect_false(ext == osi_NULL)) {
	goto error;
    }
#else /* !OSI_SMALLSTACK_ENV */
    ext = &ext_l;
#endif /* !OSI_SMALLSTACK_ENV */

    osi_trace_probe_name_extract_begin(ext, probe_name);

    osi_rwlock_WrLock(&osi_trace_directory.lock);

    n.intermed = &osi_trace_directory.root;
    tok = osi_trace_probe_name_extract_next_token(ext);
    ntok = osi_trace_probe_name_extract_next_token(ext);

    for ( ; 
	 tok;
	 tok = ntok, ntok = osi_trace_probe_name_extract_next_token(ext)) {

	/* if we're at a leaf node and still have more probe name tokens
	 * then the name space specification is illegal */
	if (n.node->node_type == OSI_TRACE_DIRECTORY_NODE_LEAF) {
	    res = OSI_TRACE_DIRECTORY_ERROR_PROBE_NAME_INVALID;
	    break;
	}

	/* scan all the children looking for this token */
	res = osi_trace_directory_search_token(n, tok, &n);
	if (OSI_RESULT_FAIL(res)) {
	    if (ntok) {
		res = osi_trace_directory_add_intermediate_node(n.intermed, 
								&n.intermed, 
								tok);
	    } else {
		res = osi_trace_directory_add_leaf_node(n.intermed,
							&n.leaf,
							tok,
							probe);
	    }
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		break;
	    }
	}
    }

    if (OSI_RESULT_OK_LIKELY(res)) {
	res = osi_trace_probe_id(probe, &probe_id);
	if (OSI_RESULT_OK_LIKELY(res)) {
	    osi_trace_directory.probes[probe_id] = n.leaf;
	}
    }
    osi_rwlock_Unlock(&osi_trace_directory.lock);

 error:
#if defined(OSI_SMALLSTACK_ENV)
    if (osi_compiler_expect_true(ext != osi_NULL)) {
	osi_mem_free(ext, sizeof(struct osi_trace_probe_name_extractor));
    }
#endif /* OSI_SMALLSTACK_ENV */
    return res;
}

/*
 * externally visible interfaces
 */

/* allocate a new probe id -- prototype only exposed to generator library */
osi_result
osi_trace_directory_probe_id_alloc(osi_trace_probe_id_t * probe_id)
{
#if defined(OSI_TRACE_DIRECTORY_ATOMIC_PROBE_ID_ALLOC)
    *probe_id = osi_atomic_inc_32_nv(&osi_trace_directory.id_counter);
#else
    osi_rwlock_WrLock(&osi_trace_directory.lock);
    *probe_id = ++osi_trace_directory.id_counter;
    osi_rwlock_Unlock(&osi_trace_directory.lock);
#endif
    return OSI_OK;
}

/* map a probe name to a probe handle */
osi_result
osi_trace_directory_N2P(const char * probe_name, osi_trace_probe_t * probe)
{
    osi_result res = OSI_FAIL;
    osi_trace_directory_node_ptr_t p;
    struct osi_trace_probe_name_extractor * ext;
    char * tok;
    int leaf_found;
#if !defined(OSI_SMALLSTACK_ENV)
    struct osi_trace_probe_name_extractor ext_l;
#endif

#if defined(OSI_SMALLSTACK_ENV)
    ext = (struct osi_trace_probe_name_extractor *)
	osi_mem_alloc(sizeof(struct osi_trace_probe_name_extractor));
    if (osi_compiler_expect_false(ext == osi_NULL)) {
	goto error;
    }
#else /* !OSI_SMALLSTACK_ENV */
    ext = &ext_l;
#endif /* !OSI_SMALLSTACK_ENV */

    osi_trace_probe_name_extract_begin(ext, probe_name);

    osi_rwlock_RdLock(&osi_trace_directory.lock);

    p.intermed = &osi_trace_directory.root;
    leaf_found = 0;
    for (tok = osi_trace_probe_name_extract_next_token(ext);
	 tok;
	 tok = osi_trace_probe_name_extract_next_token(ext)) {

	if (p.node->node_type == OSI_TRACE_DIRECTORY_NODE_LEAF) {
	    res = OSI_FAIL;
	    break;
	}

	res = osi_trace_directory_search_token(p, tok, &p);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    break;
	}
    }

    if (OSI_RESULT_OK_LIKELY(res)) {
	*probe = p.leaf->probe;
    }

    osi_rwlock_Unlock(&osi_trace_directory.lock);

 error:
#if defined(OSI_SMALLSTACK_ENV)
    if (osi_compiler_expect_true(ext != osi_NULL)) {
	osi_mem_free(ext, sizeof(struct osi_trace_probe_name_extractor));
    }
#endif /* OSI_SMALLSTACK_ENV */
    return res;
}

/* map a probe id to its primary name */
osi_result
osi_trace_directory_I2N(osi_trace_probe_id_t probe_id, char * probe_name, size_t len)
{
    osi_result res = OSI_OK;
    osi_trace_directory_node_ptr_t n;
    char ** tokens;
#if !defined(OSI_SMALLSTACK_ENV)
    char * tokens_l[OSI_TRACE_MAX_PROBE_TREE_DEPTH];
#endif
    int depth = OSI_TRACE_MAX_PROBE_TREE_DEPTH;

    probe_name[0] = '\0';


#if defined(OSI_SMALLSTACK_ENV)
    tokens = (char **) osi_mem_alloc(sizeof(char *) * OSI_TRACE_MAX_PROBE_TREE_DEPTH);
    if (osi_compiler_expect_false(tokens == osi_NULL)) {
	goto error;
    }
#else /* !OSI_SMALLSTACK_ENV */
    tokens = tokens_l;
#endif /* !OSI_SMALLSTACK_ENV */

    osi_rwlock_RdLock(&osi_trace_directory.lock);
    if (osi_compiler_expect_true(probe_id <= osi_trace_directory.id_counter)) {
	/* walk up the tree collecting all the token pointers */
	for (n.leaf = osi_trace_directory.probes[probe_id]; 
	     n.intermed ; 
	     n.intermed = n.node->parent) {
	    tokens[--depth] = n.node->probe_name;
	}

	/* concatenate them into the buffer */
	for (; depth < OSI_TRACE_MAX_PROBE_TREE_DEPTH; depth++) {
	    osi_string_lcat(probe_name, tokens[depth], len);
	    if (depth != (OSI_TRACE_MAX_PROBE_TREE_DEPTH-1)) {
		osi_string_lcat(probe_name, ".", len);
	    }
	}
    }
    osi_rwlock_Unlock(&osi_trace_directory.lock);

 error:
#if defined(OSI_SMALLSTACK_ENV)
    if (osi_compiler_expect_true(tokens != osi_NULL)) {
	osi_mem_free(tokens, sizeof(char *) * OSI_TRACE_MAX_PROBE_TREE_DEPTH);
    }
#endif /* OSI_SMALLSTACK_ENV */
    return res;
}

/* map a probe id to its probe handle */
osi_result
osi_trace_directory_I2P(osi_trace_probe_id_t probe_id, osi_trace_probe_t * probe)
{
    osi_result res = OSI_OK;

    osi_rwlock_RdLock(&osi_trace_directory.lock);
    if (osi_compiler_expect_true(probe_id <= osi_trace_directory.id_counter)) {
	*probe = osi_trace_directory.probes[probe_id]->probe;
    }
    osi_rwlock_Unlock(&osi_trace_directory.lock);

 error:
    return res;
}


osi_result
osi_trace_directory_foreach(const char * filter,
			    osi_trace_directory_foreach_action_t * fp,
			    void * data)
{
    osi_result res = OSI_OK;
    struct osi_trace_directory_tree_walker * walk;
    struct osi_trace_probe_name_extractor * ext;
#if !defined(OSI_SMALLSTACK_ENV)
    struct osi_trace_directory_tree_walker walk_l;
    struct osi_trace_probe_name_extractor ext_l;
#endif

#if defined(OSI_SMALLSTACK_ENV)
    walk = osi_mem_alloc(sizeof(struct osi_trace_directory_tree_walker));
    if (osi_compiler_expect_false(walk == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }
    ext = osi_mem_alloc(sizeof(struct osi_trace_probe_name_extractor));
    if (osi_compiler_expect_false(ext == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }
#else
    walk = &walk_l;
    ext = &ext_l;
#endif

    res = osi_trace_directory_walk_begin(walk, ext, filter, fp, data);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_directory_walk(walk);

 error:
#if defined(OSI_SMALLSTACK_ENV)
    if (osi_compiler_expect_true(walk != osi_NULL)) {
	osi_mem_free(walk, sizeof(struct osi_trace_directory_tree_walker));
    }
    if (osi_compiler_expect_true(ext != osi_NULL)) {
	osi_mem_free(ext, sizeof(struct osi_trace_probe_name_extractor));
    }
#endif
    return res;
}

/*
 * register the primary name for a probe
 */
osi_result
osi_trace_directory_probe_register(const char * probe_name, osi_trace_probe_t probe)
{
    return osi_trace_directory_probe_register_internal(probe_name, 
						       probe, 
						       1 /* register as primary */);
}

/*
 * regiser an alias for a probe
 */
osi_result
osi_trace_directory_probe_register_alias(const char * probe_name, osi_trace_probe_t probe)
{
    return osi_trace_directory_probe_register_internal(probe_name,
						       probe,
						       0 /* register as alias */);
}


/*
 * probe directory
 * initialization routines
 */
osi_result
osi_trace_directory_PkgInit(void)
{
    osi_result code = OSI_OK;
    osi_rwlock_options_t opt;
    int i;

    osi_mem_zero(&opt, sizeof(opt));
    opt.preemptive_only = 1;
    osi_rwlock_Init(&osi_trace_directory.lock, &opt);

    osi_trace_directory.id_counter = 0;
    osi_trace_directory.magic = 0xA798897A;

    for (i = 0; i < OSI_TRACE_PROBES_MAX; i++) {
	osi_trace_directory.probes[i] = osi_NULL;
    }

    for (i = 0; i < OSI_TRACE_PROBE_NAME_HASH_SIZE; i++) {
	osi_trace_directory.root.hdr.node_type = 
	    OSI_TRACE_DIRECTORY_NODE_ROOT;
	osi_trace_directory.root.hdr.parent = osi_NULL;
	osi_list_Init(&osi_trace_directory.root.hash_chains[i]);
    }

#if defined(OSI_USERSPACE_ENV)
    code = osi_trace_directory_msg_PkgInit();
#endif

    return code;
}

osi_result
osi_trace_directory_PkgShutdown(void)
{
    osi_result code = OSI_OK;
#if defined(OSI_USERSPACE_ENV)
    code = osi_trace_directory_msg_PkgShutdown();
#endif
    return code;
}
